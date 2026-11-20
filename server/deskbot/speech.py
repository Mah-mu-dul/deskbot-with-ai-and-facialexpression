"""Speech-to-text (faster-whisper, local) and text-to-speech (edge-tts).

Audio exchanged with the bot is always 16 kHz, 16-bit, mono WAV.
"""
from __future__ import annotations

import asyncio
import io
import logging
import os
import re
import threading
import wave

import numpy as np

log = logging.getLogger("deskbot.speech")
SAMPLE_RATE = 16000


def pcm_to_wav(pcm: bytes, rate: int = SAMPLE_RATE) -> bytes:
    buf = io.BytesIO()
    with wave.open(buf, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(pcm)
    return buf.getvalue()


def silence_wav(ms: int = 200) -> bytes:
    return pcm_to_wav(b"\x00\x00" * (SAMPLE_RATE * ms // 1000))


def wav_to_float(wav_bytes: bytes) -> np.ndarray:
    with wave.open(io.BytesIO(wav_bytes), "rb") as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    if width != 2:
        raise ValueError("expected 16-bit WAV")
    audio = np.frombuffer(raw, np.int16).astype(np.float32) / 32768.0
    if ch > 1:
        audio = audio.reshape(-1, ch).mean(axis=1)
    if rate != SAMPLE_RATE:
        n = int(len(audio) * SAMPLE_RATE / rate)
        audio = np.interp(np.linspace(0, len(audio), n, endpoint=False), np.arange(len(audio)), audio)
    # normalise quiet microphones
    peak = float(np.max(np.abs(audio))) if audio.size else 0.0
    if 0 < peak < 0.5:
        audio = audio * (0.5 / peak)
    return audio.astype(np.float32)


class Speech:
    def __init__(self, whisper_model: str, language: str | None, voice: str, rate: str, models_dir=None):
        self.whisper_name = whisper_model
        self.language = language
        self.voice = voice
        self.rate = rate
        self.models_dir = models_dir
        self._whisper = None
        self._lock = threading.Lock()
        self._tts_lock = threading.Lock()
        self._cache: dict[tuple, bytes] = {}       # (text, voice) -> wav, for repeated phrases

    # ------------------------------------------------------------- STT
    def _model(self):
        with self._lock:
            if self._whisper is None:
                from faster_whisper import WhisperModel
                local = self.models_dir / f"whisper-{self.whisper_name}" if self.models_dir else None
                if local is not None and (local / "model.bin").exists():
                    log.info("Loading Whisper model from %s (offline)", local)
                    self._whisper = WhisperModel(str(local), device="cpu", compute_type="int8")
                else:
                    log.info("Loading Whisper model '%s' (first time downloads it)...", self.whisper_name)
                    self._whisper = WhisperModel(self.whisper_name, device="cpu", compute_type="int8")
            return self._whisper

    def preload(self) -> None:
        try:
            import faster_whisper  # noqa: F401
        except ImportError:
            log.info("Speech-to-text not installed: voice input is off, text chat works")
            return
        try:
            self._model()
        except Exception as e:  # noqa: BLE001
            log.error("Whisper failed to load: %s", e)

    def has_speech(self, wav_bytes: bytes) -> bool:
        """Fast check (tens of ms) whether a clip contains any speech at all."""
        try:
            from faster_whisper.vad import VadOptions, get_speech_timestamps
            ts = get_speech_timestamps(wav_to_float(wav_bytes), VadOptions(min_speech_duration_ms=250))
            return sum(t["end"] - t["start"] for t in ts) >= SAMPLE_RATE * 0.3
        except Exception as e:  # noqa: BLE001
            log.warning("Speech check failed (%s): continuing", e)
            return True

    def transcribe_detect(self, wav_bytes: bytes, hotwords: str | None = None,
                          max_tokens: int | None = None) -> tuple[str, float, float]:
        """One pass: transcript plus the English / Bangla probabilities.
        max_tokens caps the text, so a noisy clip can't keep the CPU busy."""
        audio = wav_to_float(wav_bytes)
        extra = {"max_new_tokens": max_tokens} if max_tokens else {}
        segments, info = self._model().transcribe(
            audio, language=None, beam_size=1, vad_filter=False,
            condition_on_previous_text=False, without_timestamps=True, hotwords=hotwords or None, **extra)
        text = " ".join(s.text.strip() for s in segments).strip()
        probs = dict(info.all_language_probs or [])
        return text, float(probs.get("en", 0.0)), float(probs.get("bn", 0.0))

    def detect_en_bn(self, wav_bytes: bytes) -> tuple[float, float]:
        """Probability that the speech is English / Bangla (fast, local)."""
        try:
            _, _, probs = self._model().detect_language(wav_to_float(wav_bytes))
            p = dict(probs)
            return float(p.get("en", 0.0)), float(p.get("bn", 0.0))
        except Exception as e:  # noqa: BLE001
            log.warning("Language detection failed: %s", e)
            return 1.0, 0.0

    def transcribe(self, wav_bytes: bytes, language: str | None = None, hotwords: str | None = None) -> str:
        audio = wav_to_float(wav_bytes)
        # The bot already trims silence, so no extra filter (it also clipped first words).
        # `hotwords` (the bot's name) helps the model spell an unusual name correctly.
        segments, _info = self._model().transcribe(
            audio, language=language or self.language, beam_size=1, vad_filter=False,
            condition_on_previous_text=False, without_timestamps=True, hotwords=hotwords or None)
        return " ".join(s.text.strip() for s in segments).strip()

    # ------------------------------------------------------------- TTS
    async def tts(self, text: str, voice: str | None = None) -> bytes | None:
        """16 kHz mono WAV. Online voices first; Windows' offline voice for English if that fails."""
        if not text.strip():
            return None
        key = (text, voice)
        if key in self._cache:
            return self._cache[key]
        if voice == "offline" and not re.search(r"[ঀ-৿]", text):
            return await asyncio.to_thread(self._offline_tts, text)      # instant Windows voice
        bangla = bool(re.search(r"[\u0980-\u09FF]", text))
        try:
            # English falls back to the instant offline voice if the online one is slow
            wav = await asyncio.wait_for(self._edge_tts(text, voice), timeout=10)
            if len(self._cache) > 200:
                self._cache.clear()
            self._cache[key] = wav
            return wav
        except Exception as e:  # noqa: BLE001 - offline, DNS failure, timeout...
            log.warning("Online voice unavailable (%r), using the offline voice", e)
        if re.search(r"[ঀ-৿]", text):
            log.warning("No offline Bangla voice: the bot shows the text without speaking")
            return None
        try:
            return await asyncio.to_thread(self._offline_tts, text)
        except Exception as e:  # noqa: BLE001
            log.error("Offline voice failed: %s", e)
            return None

    async def _edge_tts(self, text: str, voice: str | None) -> bytes | None:
        import edge_tts
        import miniaudio

        communicate = edge_tts.Communicate(text, voice or self.voice, rate=self.rate)
        mp3 = bytearray()
        async for chunk in communicate.stream():
            if chunk["type"] == "audio":
                mp3 += chunk["data"]
        if not mp3:
            raise RuntimeError("no audio returned")
        decoded = miniaudio.decode(bytes(mp3), output_format=miniaudio.SampleFormat.SIGNED16,
                                   nchannels=1, sample_rate=SAMPLE_RATE)
        return pcm_to_wav(decoded.samples.tobytes())

    def _offline_tts(self, text: str) -> bytes | None:
        """Windows built-in voice (SAPI) via pyttsx3, converted to 16 kHz mono."""
        import tempfile

        import miniaudio
        import pyttsx3

        with self._tts_lock:
            try:
                import comtypes
                comtypes.CoInitialize()          # SAPI needs COM on this worker thread
            except Exception:  # noqa: BLE001
                pass
            fd, path = tempfile.mkstemp(suffix=".wav")
            os.close(fd)
            try:
                engine = pyttsx3.init()
                voices = engine.getProperty("voices")
                female = [v for v in voices if "zira" in v.name.lower() or "female" in v.name.lower()]
                if female:
                    engine.setProperty("voice", female[0].id)
                engine.setProperty("rate", 175)
                engine.save_to_file(text, path)
                engine.runAndWait()
                engine.stop()
                decoded = miniaudio.decode_file(path, output_format=miniaudio.SampleFormat.SIGNED16,
                                                nchannels=1, sample_rate=SAMPLE_RATE)
                return pcm_to_wav(decoded.samples.tobytes())
            finally:
                try:
                    os.remove(path)
                except OSError:
                    pass
