"""In-memory state of the bot + persisted settings, stats and chat."""
from __future__ import annotations

import json
import logging
import threading
import time
import uuid
from collections import Counter, OrderedDict, deque
from datetime import date
from pathlib import Path

log = logging.getLogger("deskbot.store")

DEFAULT_SETTINGS = {
    "bot_name": "Desky",
    "persona": "You love helping students stay focused and you care about their wellbeing.",
    "ai_provider": "gemini",      # gemini | claude
    "gemini_api_key": "",
    "gemini_model": "gemini-flash-lite-latest",
    "claude_api_key": "",
    "mood_sounds": True,          # bot chirps when its expression changes
    "hands_free": True,           # always listening; answers when called by name
    "wake_phrase": "",            # empty = "Hey <bot name>"
    "wake_words": "অলি",          # extra names, comma separated (the bot's name always works)
    "language": "auto",           # auto | en | bn  (auto = follow the user)
    "voice_en": "en-US-AnaNeural",
    "voice_bn": "bn-BD-NabanitaNeural",
    "emotion_talk": True,         # comment on the user's mood
    "emotion_cooldown_s": 300,
    "phone_warning": True,        # warn about phone use during focus
    "phone_cooldown_s": 45,
    "phone_min_hits": 2,          # phone seen in N of the last 5 frames
    "phone_confidence": 0.35,
    "away_warning": True,         # remind if user leaves desk during focus
    "away_after_s": 180,
    "climate_tips": True,
    "greet_on_return": True,
    "work_min": 25,
    "break_min": 5,
}


def _today() -> str:
    return date.today().isoformat()


def _empty_day() -> dict:
    return {"focus_sessions": 0, "focus_seconds": 0, "phone_warnings": 0,
            "voice_chats": 0, "away_warnings": 0, "emotions": {}}


class Store:
    def __init__(self, data_dir: Path):
        self.lock = threading.RLock()
        self.path = data_dir / "state.json"
        data_dir.mkdir(parents=True, exist_ok=True)

        self.device: dict = {"online": False, "last_seen": 0.0, "temp": None, "hum": None,
                             "focus": "off", "focus_left": 0, "focus_total": 0, "work_min": 25,
                             "break_min": 5, "sessions": 0, "busy": False, "volume": 70,
                             "face": "neutral", "camera": False, "ip": ""}
        self.vision: dict = {"face": False, "emotion": None, "scores": {}, "phone": False,
                             "phone_conf": 0.0, "person": False, "ms": 0, "ts": 0.0}
        self.latest_jpeg: bytes | None = None
        self.annotated_jpeg: bytes | None = None

        self.emotion_window: deque[str | None] = deque(maxlen=5)
        self.phone_window: deque[bool] = deque(maxlen=5)
        self.emotion_log: deque[tuple[float, str]] = deque(maxlen=4000)
        self.sensor_log: deque[tuple[float, float, float]] = deque(maxlen=2880)  # 24 h @ 30 s
        self.events: deque[dict] = deque(maxlen=100)                            # activity feed
        self.chat: deque[dict] = deque(maxlen=200)
        self.pending: deque[dict] = deque()
        self.tts_cache: OrderedDict[str, bytes] = OrderedDict()

        self.settings: dict = dict(DEFAULT_SETTINGS)
        self.sessions: list[dict] = []           # finished focus sessions (saved)
        self.current_session: dict | None = None  # the one running now
        self.stats: dict[str, dict] = {}
        self.last_person_ts = 0.0
        self._last_sensor_log = 0.0
        self._last_frame_ts = 0.0
        self._load()

    # ------------------------------------------------------------ persistence
    def _load(self) -> None:
        if not self.path.exists():
            return
        try:
            data = json.loads(self.path.read_text("utf-8"))
            self.settings.update({k: v for k, v in data.get("settings", {}).items() if k in DEFAULT_SETTINGS})
            self.stats = data.get("stats", {})
            self.sessions = data.get("sessions", [])
            self.chat.extend(data.get("chat", [])[-200:])
        except Exception as e:  # noqa: BLE001
            log.warning("Could not read %s: %s", self.path, e)

    def save(self) -> None:
        with self.lock:
            data = {"settings": self.settings, "stats": self.stats, "chat": list(self.chat),
                    "sessions": self.sessions[-1000:]}
        tmp = self.path.with_suffix(".tmp")
        tmp.write_text(json.dumps(data, indent=1), "utf-8")
        tmp.replace(self.path)

    # ------------------------------------------------------------ stats
    def day(self) -> dict:
        with self.lock:
            return self.stats.setdefault(_today(), _empty_day())

    def bump(self, key: str, n: int = 1) -> None:
        with self.lock:
            d = self.day()
            d[key] = d.get(key, 0) + n
            if self.current_session is not None and key in ("phone_warnings", "away_warnings"):
                self.current_session[key] = self.current_session.get(key, 0) + n

    # ------------------------------------------------------------ focus sessions
    def start_session(self, planned_min: int) -> None:
        with self.lock:
            self.current_session = {"start": time.time(), "date": _today(), "planned_min": planned_min,
                                    "focused_s": 0.0, "phone_warnings": 0, "away_warnings": 0, "moods": {}}

    def end_session(self, completed: bool) -> dict | None:
        """Close the running session. Sessions under 30 s of focus aren't kept."""
        with self.lock:
            s, self.current_session = self.current_session, None
            if not s:
                return None
            s["end"] = time.time()
            s["completed"] = completed
            s["focused_s"] = round(s["focused_s"])
            s["moods"] = {k: round(v) for k, v in s["moods"].items()}
            if s["focused_s"] >= 30:
                self.sessions.append(s)
                self.sessions = self.sessions[-1000:]
                return s
            return None

    def all_sessions(self) -> list[dict]:
        with self.lock:
            cur = self.current_session
            running = [dict(cur, end=None, completed=False)] if cur and cur["focused_s"] >= 30 else []
            return list(self.sessions) + running

    def log_event(self, kind: str, text: str) -> None:
        with self.lock:
            self.events.appendleft({"ts": time.time(), "kind": kind, "text": text})

    def add_chat(self, role: str, text: str, source: str) -> None:
        with self.lock:
            self.chat.append({"role": role, "text": text, "source": source, "ts": time.time()})

    def history_for_llm(self) -> list[dict]:
        """Recent conversation as alternating user/assistant messages."""
        with self.lock:
            msgs: list[dict] = []
            for m in list(self.chat)[-16:]:
                if m["role"] not in ("user", "assistant"):
                    continue
                if msgs and msgs[-1]["role"] == m["role"]:
                    msgs[-1]["content"] += "\n" + m["text"]
                else:
                    msgs.append({"role": m["role"], "content": m["text"]})
            while msgs and msgs[0]["role"] != "user":
                msgs.pop(0)
            if msgs and msgs[-1]["role"] == "user":     # current turn is appended by the caller
                msgs.pop()
            return msgs

    # ------------------------------------------------------------ device
    def update_device(self, h) -> float:
        """Parse telemetry headers; returns seconds since previous frame."""
        now = time.time()

        def num(key, cast=float):
            try:
                return cast(h.get(key)) if h.get(key) not in (None, "") else None
            except ValueError:
                return None

        with self.lock:
            dt = min(5.0, now - self._last_frame_ts) if self._last_frame_ts else 0.0
            self._last_frame_ts = now
            d = self.device
            d["last_seen"] = now
            d["online"] = True
            t, hu = num("x-temp"), num("x-hum")
            if t is not None:
                d["temp"], d["hum"] = t, hu
            d["focus"] = h.get("x-focus", d["focus"])
            for key, hdr in (("focus_left", "x-focus-left"), ("focus_total", "x-focus-total"),
                             ("work_min", "x-focus-work"), ("break_min", "x-focus-break"),
                             ("sessions", "x-sessions"), ("volume", "x-volume")):
                v = num(hdr, int)
                if v is not None:
                    d[key] = v
            d["busy"] = h.get("x-busy") == "1"
            d["face"] = h.get("x-face", d["face"])
            d["camera"] = h.get("x-camera") == "1"
            d["ip"] = h.get("x-ip", d["ip"])
            d["wifi"] = h.get("x-wifi", d.get("wifi", ""))
            rssi = num("x-rssi", int)
            if rssi is not None:
                d["rssi"] = rssi

            if d["focus"] == "work":
                self.day()["focus_seconds"] += dt
                if self.current_session is None:
                    self.start_session(d.get("work_min") or 25)
                self.current_session["focused_s"] += dt
            if t is not None and now - self._last_sensor_log >= 30:
                self._last_sensor_log = now
                self.sensor_log.append((now, t, hu if hu is not None else 0.0))
            return dt

    def update_vision(self, result: dict, jpeg: bytes, dt: float) -> None:
        now = time.time()
        with self.lock:
            self.latest_jpeg = jpeg
            self.annotated_jpeg = result.get("annotated", jpeg)
            self.vision = {k: v for k, v in result.items() if k != "annotated"}
            self.vision["ts"] = now
            self.emotion_window.append(result.get("emotion") if result.get("face") else None)
            self.phone_window.append(bool(result.get("phone")))
            if result.get("person") or result.get("face"):
                self.last_person_ts = now
            emo = result.get("emotion")
            if result.get("face") and emo:
                self.emotion_log.append((now, emo))
                em = self.day()["emotions"]
                em[emo] = round(em.get(emo, 0) + dt, 1)
                if self.current_session is not None and self.device.get("focus") == "work":
                    m = self.current_session["moods"]
                    m[emo] = m.get(emo, 0) + dt

    def stable_emotion(self) -> str | None:
        """Emotion seen in at least 3 of the last 5 frames."""
        with self.lock:
            c = Counter(e for e in self.emotion_window if e)
            if not c:
                return None
            emo, n = c.most_common(1)[0]
            return emo if n >= 3 else None

    def phone_hits(self) -> int:
        with self.lock:
            return sum(self.phone_window)

    # ------------------------------------------------------------ commands & audio
    def queue_cmd(self, cmd: dict) -> None:
        with self.lock:
            self.pending.append(cmd)

    def take_cmds(self, limit: int = 4) -> list[dict]:
        with self.lock:
            out = []
            while self.pending and len(out) < limit:
                out.append(self.pending.popleft())
            return out

    def put_audio(self, wav: bytes) -> str:
        key = uuid.uuid4().hex[:12]
        with self.lock:
            self.tts_cache[key] = wav
            while len(self.tts_cache) > 40:
                self.tts_cache.popitem(last=False)
        return key

    def get_audio(self, key: str) -> bytes | None:
        with self.lock:
            return self.tts_cache.get(key)
