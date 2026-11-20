"""Environment configuration."""
from __future__ import annotations

import os
import sys
from dataclasses import dataclass
from pathlib import Path

from dotenv import load_dotenv

if getattr(sys, "frozen", False):
    # Packaged DeskBot.exe: settings/data live next to the exe, the dashboard and
    # models are inside the bundle.
    APP_DIR = Path(sys.executable).resolve().parent
    RES_DIR = Path(getattr(sys, "_MEIPASS", APP_DIR))
else:
    APP_DIR = RES_DIR = Path(__file__).resolve().parent.parent
ROOT = APP_DIR
load_dotenv(APP_DIR / ".env")


@dataclass(frozen=True)
class Config:
    claude_model: str = os.getenv("CLAUDE_MODEL", "claude-opus-5-5")
    claude_effort: str = os.getenv("CLAUDE_EFFORT", "low")
    whisper_model: str = os.getenv("WHISPER_MODEL", "base")
    whisper_language: str | None = os.getenv("WHISPER_LANGUAGE") or None
    tts_voice: str = os.getenv("TTS_VOICE", "en-US-AriaNeural")
    tts_rate: str = os.getenv("TTS_RATE", "+5%")
    host: str = os.getenv("HOST", "0.0.0.0")
    port: int = int(os.getenv("PORT", "8000"))
    device_url: str = os.getenv("DEVICE_URL", "http://deskbot.local")
    models_dir: Path = RES_DIR / "models"
    data_dir: Path = APP_DIR / "data"
    static_dir: Path = RES_DIR / "static"


CONFIG = Config()
