"""Face emotion + phone detection on camera frames from the bot.

* Face detection: OpenCV Haar cascade (ships with opencv-python).
* Emotion:        FER+ ONNX model (run download_models.py once).
* Phone/person:   YOLOv8n via ultralytics (COCO class 67 = cell phone).
Every part degrades gracefully if its model is missing.
"""
from __future__ import annotations

import logging
import time
from pathlib import Path

import cv2
import numpy as np

log = logging.getLogger("deskbot.vision")

FERPLUS_LABELS = ["neutral", "happy", "surprised", "sad", "angry", "disgust", "fear", "contempt"]
COCO_PERSON, COCO_PHONE = 0, 67

EMOTION_COLORS = {
    "neutral": (200, 200, 200), "happy": (80, 220, 120), "surprised": (60, 220, 255),
    "sad": (255, 140, 60), "angry": (60, 60, 255), "disgust": (60, 160, 120),
    "fear": (200, 80, 200), "contempt": (150, 150, 150),
}


class Vision:
    def __init__(self, models_dir: Path, enable_phone: bool = True):
        self.face_cascade = cv2.CascadeClassifier(
            cv2.data.haarcascades + "haarcascade_frontalface_default.xml")

        self.emotion = None
        model_path = models_dir / "emotion-ferplus-8.onnx"
        if model_path.exists():
            try:
                import onnxruntime as ort
                self.emotion = ort.InferenceSession(str(model_path), providers=["CPUExecutionProvider"])
                self.emotion_input = self.emotion.get_inputs()[0].name
                log.info("Emotion model loaded")
            except Exception as e:  # noqa: BLE001
                log.warning("Emotion model failed to load: %s", e)
        else:
            log.warning("No emotion model at %s - run download_models.py", model_path)

        # Phone / person detector: YOLOv8n as ONNX (no PyTorch needed). Falls back to
        # ultralytics + yolov8n.pt only if the ONNX file is missing.
        self.yolo = None          # onnxruntime session
        self.yolo_ul = None       # ultralytics model (fallback)
        if enable_phone:
            onnx_path = models_dir / "yolov8n.onnx"
            if onnx_path.exists():
                try:
                    import onnxruntime as ort
                    self.yolo = ort.InferenceSession(str(onnx_path), providers=["CPUExecutionProvider"])
                    self.yolo_input = self.yolo.get_inputs()[0].name
                    log.info("YOLOv8n (ONNX) loaded for phone detection")
                except Exception as e:  # noqa: BLE001
                    log.warning("YOLO ONNX failed to load: %s", e)
            if self.yolo is None:
                try:
                    from ultralytics import YOLO
                    pt = models_dir / "yolov8n.pt"
                    self.yolo_ul = YOLO(str(pt) if pt.exists() else "yolov8n.pt")
                    log.info("YOLOv8n (ultralytics) loaded for phone detection")
                except Exception as e:  # noqa: BLE001
                    log.warning("Phone detection disabled: no models/yolov8n.onnx and no ultralytics (%s)", e)

    @property
    def capabilities(self) -> dict:
        return {"emotion": self.emotion is not None, "phone": self.yolo is not None or self.yolo_ul is not None}

    def _detect(self, img: np.ndarray) -> list[tuple[int, float, tuple[int, int, int, int]]]:
        """People and phones as (class, confidence, (x1, y1, x2, y2)) in image pixels."""
        if self.yolo is None:
            if self.yolo_ul is None:
                return []
            res = self.yolo_ul.predict(img, imgsz=640, conf=0.25, classes=[COCO_PERSON, COCO_PHONE], verbose=False)[0]
            return [(int(b.cls[0]), float(b.conf[0]), tuple(int(v) for v in b.xyxy[0])) for b in res.boxes]

        # letterbox to 640x640, keeping the aspect ratio
        h, w = img.shape[:2]
        scale = 640 / max(h, w)
        nw, nh = int(round(w * scale)), int(round(h * scale))
        px, py = (640 - nw) // 2, (640 - nh) // 2
        canvas = np.full((640, 640, 3), 114, np.uint8)
        canvas[py:py + nh, px:px + nw] = cv2.resize(img, (nw, nh))
        blob = cv2.cvtColor(canvas, cv2.COLOR_BGR2RGB).transpose(2, 0, 1)[None].astype(np.float32) / 255.0

        pred = self.yolo.run(None, {self.yolo_input: blob})[0][0].T     # (8400, 84): cx, cy, w, h, 80 scores
        found = []
        for cls in (COCO_PERSON, COCO_PHONE):
            conf = pred[:, 4 + cls]
            keep = conf >= 0.25
            if not keep.any():
                continue
            boxes, scores = pred[keep, :4], conf[keep]
            xywh = [[(cx - bw / 2 - px) / scale, (cy - bh / 2 - py) / scale, bw / scale, bh / scale]
                    for cx, cy, bw, bh in boxes]
            for i in np.array(cv2.dnn.NMSBoxes(xywh, scores.tolist(), 0.25, 0.45)).flatten():
                x, y, bw, bh = xywh[i]
                found.append((cls, float(scores[i]),
                              (max(0, int(x)), max(0, int(y)), min(w, int(x + bw)), min(h, int(y + bh)))))
        return found

    # ------------------------------------------------------------------
    def _emotion_of(self, gray_face: np.ndarray) -> tuple[str, dict]:
        face = cv2.resize(gray_face, (64, 64)).astype(np.float32)
        logits = self.emotion.run(None, {self.emotion_input: face.reshape(1, 1, 64, 64)})[0][0]
        e = np.exp(logits - logits.max())
        probs = e / e.sum()
        scores = {label: round(float(p), 3) for label, p in zip(FERPLUS_LABELS, probs)}
        return FERPLUS_LABELS[int(np.argmax(probs))], scores

    def analyze(self, jpeg: bytes, phone_conf: float = 0.40) -> dict:
        t0 = time.time()
        if len(jpeg) < 4 or jpeg[-2:] != bytes([0xFF, 0xD9]):     # truncated frame: don't analyse
            return {"ok": False, "truncated": True}
        img = cv2.imdecode(np.frombuffer(jpeg, np.uint8), cv2.IMREAD_COLOR)
        if img is None:
            return {"ok": False}
        out = {"ok": True, "face": False, "emotion": None, "scores": {}, "phone": False,
               "phone_conf": 0.0, "person": False, "boxes": []}

        gray = cv2.equalizeHist(cv2.cvtColor(img, cv2.COLOR_BGR2GRAY))
        faces = self.face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=6, minSize=(60, 60))
        if len(faces):
            x, y, w, h = max(faces, key=lambda f: f[2] * f[3])
            out["face"] = True
            out["person"] = True
            label = "face"
            if self.emotion is not None:
                pad = int(0.1 * w)
                crop = gray[max(0, y - pad):y + h + pad, max(0, x - pad):x + w + pad]
                emo, scores = self._emotion_of(crop)
                out["emotion"], out["scores"] = emo, scores
                label = f"{emo} {scores[emo]:.0%}"
            color = EMOTION_COLORS.get(out["emotion"] or "neutral", (200, 200, 200))
            cv2.rectangle(img, (x, y), (x + w, y + h), color, 2)
            cv2.putText(img, label, (x, max(15, y - 8)), cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

        for cls, conf, (x1, y1, x2, y2) in self._detect(img):
            if cls == COCO_PERSON and conf >= 0.4:
                out["person"] = True
            elif cls == COCO_PHONE and conf >= phone_conf:
                out["phone"] = True
                out["phone_conf"] = max(out["phone_conf"], round(conf, 2))
                cv2.rectangle(img, (x1, y1), (x2, y2), (0, 0, 255), 3)
                cv2.putText(img, f"PHONE {conf:.0%}", (x1, max(15, y1 - 8)),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)

        ok, enc = cv2.imencode(".jpg", img, [cv2.IMWRITE_JPEG_QUALITY, 75])
        out["annotated"] = enc.tobytes() if ok else jpeg
        out["ms"] = int((time.time() - t0) * 1000)
        return out
