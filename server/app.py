"""DeskBot brain server.

Run:  uvicorn app:app --host 0.0.0.0 --port 8000     (or: python app.py)
Dashboard: http://<this-pc-ip>:8000
"""
from __future__ import annotations

import logging
import threading
import time
from urllib.parse import quote

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import FileResponse, JSONResponse, Response
from fastapi.staticfiles import StaticFiles
from starlette.concurrency import run_in_threadpool

from deskbot.bot import Bot
from deskbot.config import CONFIG
from deskbot.store import DEFAULT_SETTINGS

SECRET_KEYS = ("gemini_api_key", "claude_api_key")


def public_settings() -> dict:
    """Settings for the browser: secrets are never sent, only whether they are set."""
    out = {k: v for k, v in bot.store.settings.items() if k not in SECRET_KEYS}
    for k in SECRET_KEYS:
        out[f"{k}_set"] = bool(bot.store.settings.get(k))
    return out

logging.basicConfig(level=logging.INFO, format="%(asctime)s %(name)s %(levelname)s %(message)s")
app = FastAPI(title="DeskBot brain")
bot = Bot()
threading.Thread(target=bot.speech.preload, daemon=True).start()

if CONFIG.static_dir.exists():
    app.mount("/static", StaticFiles(directory=CONFIG.static_dir), name="static")


# ------------------------------------------------------------------ device API
@app.post("/api/device/frame")
async def device_frame(request: Request):
    return await bot.handle_frame(request.headers, await request.body())


@app.post("/api/device/voice")
async def device_voice(request: Request, auto: int = 0):
    result = await bot.handle_voice(request.headers, await request.body(), auto=bool(auto))
    if result is None:                       # hands-free: not addressed to the bot
        return Response(status_code=204)
    wav, reply, user_text, face = result
    return Response(content=wav, media_type="audio/wav", headers={
        "X-Reply-Text": quote(reply[:600]), "X-User-Text": quote(user_text[:300]), "X-Face": face})


@app.get("/api/tts/{key}.wav")
def tts_audio(key: str):
    wav = bot.store.get_audio(key)
    if wav is None:
        raise HTTPException(404)
    return Response(content=wav, media_type="audio/wav")


# ------------------------------------------------------------------ dashboard API
@app.get("/")
def index():
    page = CONFIG.static_dir / "index.html"
    if page.exists():
        return FileResponse(page)
    return JSONResponse({"msg": "dashboard files missing (server/static)", "state": "/api/state"})


@app.get("/api/state")
def state():
    st = bot.store
    with st.lock:
        dev = dict(st.device)
        dev["online"] = time.time() - dev["last_seen"] < 10
        cutoff = time.time() - 3600
        return {
            "device": dev, "vision": st.vision, "stable_emotion": st.stable_emotion(),
            "today": st.day(), "settings": public_settings(), "capabilities": bot.vision.capabilities,
            "chat": list(st.chat)[-50:], "events": list(st.events)[:30],
            "emotion_log": [e for e in st.emotion_log if e[0] > cutoff][-600:],
            "sensor_log": list(st.sensor_log)[-240:],
            "history": {k: v for k, v in sorted(st.stats.items())[-14:]},
            "device_url": CONFIG.device_url, "model": CONFIG.claude_model,
            "server_url": f"http://{lan_ip()}:{CONFIG.port}",
            "ai": {**bot.brain.status, "summary": bot.ai_state()},
        }


@app.get("/api/frame.jpg")
def frame(raw: int = 0):
    img = bot.store.latest_jpeg if raw else bot.store.annotated_jpeg
    if not img:
        raise HTTPException(404, "no frame yet")
    return Response(content=img, media_type="image/jpeg", headers={"Cache-Control": "no-store"})


@app.post("/api/chat")
async def chat(body: dict):
    text = str(body.get("text", "")).strip()
    if not text:
        raise HTTPException(400, "empty message")
    return await bot.dashboard_chat(text, bool(body.get("speak", True)))


@app.post("/api/say")
async def say(body: dict):
    text = str(body.get("text", "")).strip()
    if not text:
        raise HTTPException(400, "empty text")
    await bot.say(text, body.get("face"), source="dashboard")
    return {"ok": True}


@app.post("/api/face")
def face(body: dict):
    bot.store.queue_cmd({"t": "face", "v": body.get("face", "neutral"), "a": int(body.get("seconds", 5)) * 1000})
    return {"ok": True}


@app.post("/api/focus")
def focus(body: dict):
    action = body.get("action", "start")
    cmd = {"t": "focus", "v": action}
    if action == "start":
        cmd["a"] = int(body.get("work", bot.store.settings["work_min"]))
        cmd["b"] = int(body.get("brk", bot.store.settings["break_min"]))
    bot.store.queue_cmd(cmd)
    return {"ok": True}


@app.post("/api/beep")
def beep(body: dict):
    bot.store.queue_cmd({"t": "beep", "v": body.get("pattern", "beep")})
    return {"ok": True}


@app.post("/api/volume")
def volume(body: dict):
    bot.store.queue_cmd({"t": "vol", "a": max(0, min(100, int(body.get("value", 70))))})
    return {"ok": True}


@app.get("/api/report")
def get_report(range: str = "day", date: str | None = None):
    """Focus history: range=day (one day) or week (7 days ending on `date`, default today)."""
    from deskbot import report
    return report.week_report(bot.store, date) if range == "week" else report.day_report(bot.store, date)


@app.post("/api/ai/test")
async def ai_test():
    """Send a tiny request to the AI and report whether it answered."""
    return await run_in_threadpool(bot.brain.ping)


@app.post("/api/bot/server")
def bot_server(body: dict):
    """Tell the bot to move to another DeskBot server (it checks the address first)."""
    url = str(body.get("url", "")).strip()
    if not url:
        raise HTTPException(400, "empty address")
    bot.store.queue_cmd({"t": "server", "v": url})
    bot.store.log_event("device", f"Asked the bot to move to {url}")
    return {"ok": True}


@app.post("/api/bot/wifi-setup")
def bot_wifi_setup():
    """Restart the bot into its DeskBot-Setup Wi-Fi to choose another network."""
    bot.store.queue_cmd({"t": "wifisetup"})
    bot.store.log_event("device", "Bot restarting into Wi-Fi setup")
    return {"ok": True}


@app.post("/api/screen")
def screen(body: dict):
    bot.store.queue_cmd({"t": "screen", "v": body.get("mode", "face")})
    return {"ok": True}


@app.post("/api/settings")
def settings(body: dict):
    with bot.store.lock:
        for k, v in body.items():
            if k not in DEFAULT_SETTINGS:
                continue
            if k in SECRET_KEYS:
                v = str(v).strip()
                if not v:                      # empty field = keep the saved key
                    continue
                if v == "-":                   # "-" = remove the saved key
                    v = ""
            bot.store.settings[k] = type(DEFAULT_SETTINGS[k])(v)
    bot.store.save()
    return public_settings()


def lan_ip() -> str:
    """This PC's address on the Wi-Fi (no data is sent; UDP connect only picks a route)."""
    import socket
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"


if __name__ == "__main__":
    import sys
    import webbrowser

    import uvicorn

    ip = lan_ip()
    print()
    print("  DeskBot server")
    print(f"  Dashboard:        http://localhost:{CONFIG.port}")
    print(f"  Bot server URL:   http://{ip}:{CONFIG.port}   (set this on the bot: http://deskbot.local)")
    print("  Close this window to stop the server.")
    print()
    if getattr(sys, "frozen", False):
        threading.Timer(3.0, webbrowser.open, [f"http://localhost:{CONFIG.port}"]).start()
    uvicorn.run(app, host=CONFIG.host, port=CONFIG.port, log_level="warning" if getattr(sys, "frozen", False) else "info")
