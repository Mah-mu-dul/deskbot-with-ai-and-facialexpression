"""Download the face-emotion model (FER+ ONNX, ~34 MB) and the speech model into ./models.

Run once:  python download_models.py
The YOLOv8n phone model is exported once to ONNX (see README, "Build the standalone app").
"""
from pathlib import Path
import sys
import urllib.request

MODELS = Path(__file__).parent / "models"
TARGET = MODELS / "emotion-ferplus-8.onnx"
URLS = [
    "https://github.com/onnx/models/raw/main/validated/vision/body_analysis/emotion_ferplus/model/emotion-ferplus-8.onnx",
    "https://github.com/onnx/models/raw/main/vision/body_analysis/emotion_ferplus/model/emotion-ferplus-8.onnx",
]


def download_whisper() -> None:
    """Speech-to-text model for offline voice (about 140 MB)."""
    target = MODELS / "whisper-base"
    if (target / "model.bin").exists():
        print(f"Already present: {target}")
        return
    try:
        from faster_whisper.utils import download_model
        print("Downloading the speech model (whisper-base, ~140 MB) ...")
        download_model("base", output_dir=str(target))
        print(f"Saved to {target}")
    except Exception as e:  # noqa: BLE001
        print(f"Could not download the speech model: {e}")


def main() -> int:
    MODELS.mkdir(exist_ok=True)
    download_whisper()
    if TARGET.exists() and TARGET.stat().st_size > 1_000_000:
        print(f"Already present: {TARGET}")
        return 0
    for url in URLS:
        try:
            print(f"Downloading {url} ...")
            urllib.request.urlretrieve(url, TARGET)
            if TARGET.stat().st_size > 1_000_000:
                print(f"Saved to {TARGET}")
                return 0
        except Exception as e:  # noqa: BLE001 - try the next mirror
            print(f"  failed: {e}")
    print("Could not download the emotion model. Download emotion-ferplus-8.onnx "
          "from github.com/onnx/models manually and put it in server/models/.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
