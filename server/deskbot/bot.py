"""Behaviour engine: turns frames, voice and dashboard actions into bot commands.

Bangla + English: replies follow the user's language (or a fixed one from
Settings), and every built-in line exists in both languages. The TTS voice is
picked from the text itself (Bengali script -> Bangla voice).
"""
from __future__ import annotations

import asyncio
import logging
import random
import re
import time
from datetime import datetime

from starlette.concurrency import run_in_threadpool

from . import offline, report
from .brain import ERRORS, Brain
from .config import CONFIG
from .speech import Speech, silence_wav
from .store import Store
from .vision import Vision

log = logging.getLogger("deskbot.bot")

BENGALI = re.compile(r"[ঀ-৿]")
AI_ERROR_TEXTS = {Brain.parse_tags(t)[0] for pair in ERRORS.values() for t in pair}

MOOD_FOR_EMOTION = {"happy": "happy", "sad": "sad", "angry": "sad", "surprised": "surprised",
                    "fear": "surprised", "disgust": "confused", "contempt": "confused", "neutral": "neutral"}
REACT_TO = {"happy", "sad", "angry", "fear", "surprised"}

# Built-in lines. A list means "pick one at random".
LINES = {
    "boot": {
        "en": "Hi, I'm {name}! Chat with me on the dashboard, or hold my button for focus mode.",
        "bn": "হ্যালো, আমি {name}! ড্যাশবোর্ডে আমার সাথে কথা বলো, অথবা ফোকাস মোডের জন্য বাটন চেপে ধরো।",
    },
    "focus_start": {
        "en": "Focus mode on for {work} minute{work_s}. Put your phone away, I'm watching!",
        "bn": "{work} মিনিটের ফোকাস মোড শুরু। ফোন দূরে রাখো, আমি দেখছি!",
    },
    "work_done": {
        "en": "Great job! Take a {brk} minute break.",
        "bn": "দারুণ কাজ! এবার {brk} মিনিটের বিরতি নাও।",
    },
    "break_done": {
        "en": "Break is over. Let's get back to it!",
        "bn": "বিরতি শেষ। চলো আবার পড়া শুরু করি!",
    },
    "phone": {
        "en": ["Hey! Phone down, please. You're in focus mode.",
               "I see that phone. Let's get back to studying.",
               "No scrolling now. Your future self will thank you.",
               "Phone detected! Put it away and keep going, you've got this."],
        "bn": ["এই! ফোনটা রাখো, এখন ফোকাস টাইম।",
               "ফোন দেখতে পাচ্ছি। চলো আবার পড়ায় মন দিই।",
               "এখন স্ক্রল করা না। পরে নিজেকেই ধন্যবাদ দেবে।",
               "ফোন ধরা পড়েছে! রেখে দাও, তুমি পারবে।"],
    },
    "away": {
        "en": "Hey, where did you go? Your focus timer is still running.",
        "bn": "এই, কোথায় গেলে? তোমার ফোকাস টাইমার এখনো চলছে।",
    },
    "welcome": {
        "en": "Welcome back! I missed you.",
        "bn": "ফিরে আসার জন্য স্বাগতম! তোমাকে মিস করছিলাম।",
    },
    "emo_happy": {"en": "You look happy! I love that.", "bn": "তোমাকে খুশি দেখাচ্ছে! খুব ভালো লাগছে।"},
    "emo_sad": {"en": "You seem a little down. I'm here if you want to talk.",
                "bn": "তোমাকে একটু মন খারাপ লাগছে। কথা বলতে চাইলে আমি আছি।"},
    "emo_angry": {"en": "Take a deep breath with me. Everything will be okay.",
                  "bn": "আমার সাথে একটা লম্বা শ্বাস নাও। সব ঠিক হয়ে যাবে।"},
    "emo_fear": {"en": "Hey, it's okay. You've got this.", "bn": "চিন্তা কোরো না, তুমি পারবে।"},
    "emo_surprised": {"en": "Whoa, what happened?", "bn": "ওমা, কী হলো?"},
    "hot": {"en": "It's {t} degrees in here. Drink some water and open a window.",
            "bn": "ঘরে {t} ডিগ্রি গরম। একটু পানি খাও আর জানালা খুলে দাও।"},
    "humid": {"en": "The air is very humid. A bit of ventilation would help you focus.",
              "bn": "বাতাস খুব আর্দ্র। একটু বাতাস চলাচল হলে মনোযোগ দিতে সুবিধা হবে।"},
    "dry": {"en": "The air is quite dry. Remember to drink water.",
            "bn": "বাতাস বেশ শুকনো। পানি খেতে ভুলো না।"},
    "not_heard": {"en": "Sorry, I didn't catch that. Could you say it again?",
                  "bn": "দুঃখিত, বুঝতে পারিনি। আরেকবার বলবে?"},
}

SITUATIONS = {
    "work_done": "the user just finished a focus session; congratulate them and suggest a short break",
    "welcome": "the user just came back to the desk after a long time",
}


def detect_lang(text: str) -> str:
    return "bn" if BENGALI.search(text or "") else "en"


class Bot:
    def __init__(self):
        self.store = Store(CONFIG.data_dir)
        self.vision = Vision(CONFIG.models_dir)
        self.speech = Speech(CONFIG.whisper_model, CONFIG.whisper_language, CONFIG.tts_voice, CONFIG.tts_rate,
                             CONFIG.models_dir)
        self.brain = Brain(CONFIG.claude_model, CONFIG.claude_effort, lambda: self.store.settings)
        self._say_lock = asyncio.Lock()
        self._last = {"phone": 0.0, "emotion": 0.0, "climate": 0.0, "away": 0.0}
        self._last_reacted_emotion: str | None = None
        self._away_warned = False
        self.last_lang = "en"
        self._followup_until = 0.0      # after an answer, talk without the wake word until then

    # ------------------------------------------------------------ helpers
    @property
    def s(self) -> dict:
        return self.store.settings

    def lang(self) -> str:
        """Language for spontaneous lines: the fixed setting, or the user's last language."""
        fixed = self.s.get("language", "auto")
        return fixed if fixed in ("en", "bn") else self.last_lang

    def line(self, key: str, **kw) -> str:
        options = LINES[key][self.lang()]
        text = random.choice(options) if isinstance(options, list) else options
        return text.format(name=self.s["bot_name"], **kw)

    VOICE_EN = "en-US-AnaNeural"        # the bot's voice: Ana for English
    VOICE_BN = "bn-IN-TanishaaNeural"   # and Tanishaa for Bangla

    def session_summary(self, s: dict | None, break_min: int | None) -> str:
        """Spoken report at the end of a focus session (English or Bangla, works offline)."""
        bn = self.lang() == "bn"
        if not s:
            return self.line("work_done", brk=break_min or 5)
        mins = round(s["focused_s"] / 60)
        phone, away = s.get("phone_warnings", 0), s.get("away_warnings", 0)
        moods = s.get("moods", {})
        mood = max(moods, key=moods.get) if moods else None
        day = self.store.day()
        today_min, today_n = round(day["focus_seconds"] / 60), day["focus_sessions"]
        n = lambda x: str(x).translate(str.maketrans("0123456789", "০১২৩৪৫৬৭৮৯")) if bn else str(x)
        if bn:
            parts = [f"সেশন শেষ! {n(mins)} মিনিট মনোযোগ দিয়েছ।"]
            parts.append("একবারও ফোন ধরোনি, দারুণ!" if phone == 0 else f"ফোন সতর্কতা {n(phone)} বার।")
            if mood:
                parts.append(f"বেশিরভাগ সময় তোমাকে {offline.BN_EMO.get(mood, mood)} দেখাচ্ছিল।")
            parts.append(f"আজ মোট {n(today_n)}টা সেশন, {n(today_min)} মিনিট।")
            if break_min:
                parts.append(f"এবার {n(break_min)} মিনিট বিরতি নাও।")
        else:
            parts = [f"Session done! {mins} minute{'s' if mins != 1 else ''} of focus."]
            parts.append("No phone at all, great job!" if phone == 0 else
                         f"{phone} phone warning{'s' if phone != 1 else ''}.")
            if away:
                parts.append(f"You left the desk {away} time{'s' if away != 1 else ''}.")
            if mood:
                parts.append(f"You looked mostly {mood}.")
            parts.append(f"Today so far: {today_n} session{'s' if today_n != 1 else ''}, "
                         f"{today_min} minute{'s' if today_min != 1 else ''}.")
            if break_min:
                parts.append(f"Take a {break_min} minute break.")
        return " ".join(parts)

    def voice_for(self, text: str) -> str:
        return self.VOICE_BN if detect_lang(text) == "bn" else self.VOICE_EN

    def note_user_language(self, text: str) -> None:
        self.last_lang = detect_lang(text)

    def context(self) -> str:
        d, v, day = self.store.device, self.store.vision, self.store.day()
        mood = self.store.stable_emotion() or (v.get("emotion") if v.get("face") else None)
        parts = [datetime.now().strftime("%A %H:%M"), f"user's face: {mood or 'not visible'}"]
        if d.get("temp") is not None:
            parts.append(f"room {d['temp']:.1f}C, humidity {d['hum']:.0f}%")
        if d["focus"] != "off":
            parts.append(f"focus timer: {d['focus']}, {d['focus_left'] // 60} min left")
        parts.append(f"today: {day['focus_sessions']} focus sessions, {day['phone_warnings']} phone warnings")
        return "; ".join(parts)

    async def say(self, text: str, face: str | None = None, beep: str | None = None, source: str = "bot") -> None:
        """Synthesize speech and queue it for the bot."""
        if beep:
            self.store.queue_cmd({"t": "beep", "v": beep})
        if face:
            self.store.queue_cmd({"t": "face", "v": face, "a": 4000 + len(text) * 70})
        wav = await self.speech.tts(text, self.voice_for(text))
        cmd = {"t": "say", "text": text[:190]}
        if wav:
            cmd["v"] = f"/api/tts/{self.store.put_audio(wav)}.wav"
        self.store.queue_cmd(cmd)
        self.store.add_chat("assistant", text, source)
        self.store.log_event("say", text)

    def _spawn(self, coro) -> None:
        if self._say_lock.locked():     # already talking: drop this reaction
            coro.close()
            return

        async def guarded():
            if self._say_lock.locked():
                coro.close()
                return
            async with self._say_lock:
                try:
                    await coro
                except Exception as e:  # noqa: BLE001
                    log.exception("background speech failed: %s", e)
        asyncio.create_task(guarded())

    async def _react(self, situation: str, fallback: str, face: str | None = None, beep: str | None = None):
        text, f = await run_in_threadpool(self.brain.react, situation, self.context(),
                                          self.s["bot_name"], self.s["persona"], self.lang())
        if not text or text in AI_ERROR_TEXTS:      # AI unavailable: use the built-in line
            text, f = fallback, None
        await self.say(text, face or f, beep)

    def apply_actions(self, actions: list[str]) -> None:
        for a in actions:
            parts = a.split(":")
            name = parts[0]
            arg = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 0
            if name == "focus_start":
                self.store.queue_cmd({"t": "focus", "v": "start", "a": arg or self.s["work_min"], "b": self.s["break_min"]})
            elif name in ("focus_stop", "focus_pause", "focus_resume"):
                self.store.queue_cmd({"t": "focus", "v": name.split("_")[1]})
            elif name == "volume":
                self.store.queue_cmd({"t": "vol", "a": max(0, min(100, arg))})

    # ------------------------------------------------------------ device frame
    async def handle_frame(self, headers, body: bytes) -> dict:
        dt = self.store.update_device(headers)
        if body:
            result = await run_in_threadpool(self.vision.analyze, body, float(self.s["phone_confidence"]))
            if result.get("ok"):
                self.store.update_vision(result, body, dt)
                if result.get("phone") and time.time() - self._last.get("phone_seen", 0) > 10:
                    self._last["phone_seen"] = time.time()
                    self.store.log_event("phone_seen", f"Phone seen ({result['phone_conf']:.0%})")
        for ev in filter(None, headers.get("x-event", "").split(",")):
            self._on_event(ev)
        self._evaluate()
        emo = self.store.stable_emotion()
        mood = MOOD_FOR_EMOTION.get(emo or "", "neutral")
        if not self.store.vision.get("face") and time.time() - self.store.last_person_ts > 120 \
                and self.store.device["focus"] == "off":
            mood = "sleepy"
        return {"mood": mood, "sfx": bool(self.s.get("mood_sounds", True)),
                "listen": bool(self.s.get("hands_free", True)), "ai": self.ai_state(),
                "cmds": self.store.take_cmds()}

    def ai_state(self) -> str:
        """ok | no_key | offline | busy | error | unknown - for the bot's Info screen."""
        st = self.brain.status.get("state", "unknown")
        if st == "unknown":
            key = "claude_api_key" if self.s.get("ai_provider") == "claude" else "gemini_api_key"
            return "unknown" if self.s.get(key) else "no_key"
        return st

    def _on_event(self, ev: str) -> None:
        d = self.store.device
        self.store.log_event("device", ev)
        if ev == "boot":
            self._spawn(self.say(self.line("boot"), "happy"))
        elif ev == "focus_start":
            self._away_warned = False
            self.store.start_session(d["work_min"])
            self._spawn(self.say(self.line("focus_start", work=d["work_min"],
                                           work_s="" if d["work_min"] == 1 else "s"), "focused"))
        elif ev == "work_done":
            self.store.bump("focus_sessions")
            s = self.store.end_session(completed=True)
            self.store.save()
            self._spawn(self.say(self.session_summary(s, d["break_min"]), "happy"))
        elif ev == "break_done":
            self.store.start_session(d["work_min"])
            self._spawn(self.say(self.line("break_done"), "surprised"))
        elif ev == "focus_stop":
            s = self.store.end_session(completed=False)
            self.store.save()
            if s and s["focused_s"] >= 60:
                self._spawn(self.say(self.session_summary(s, None), "neutral"))

    def _evaluate(self) -> None:
        now, d, s = time.time(), self.store.device, self.s
        focus = d["focus"]

        # 1) phone during focus
        if focus == "work" and s["phone_warning"] and self.store.phone_hits() >= int(s["phone_min_hits"]) \
                and now - self._last["phone"] > float(s["phone_cooldown_s"]):
            self._last["phone"] = now
            self.store.bump("phone_warnings")
            self.store.log_event("phone", "Phone use detected during focus")
            self.store.queue_cmd({"t": "beep", "v": "warning"})
            self.store.queue_cmd({"t": "face", "v": "warning", "a": 6000})
            self._spawn(self.say(self.line("phone"), "angry"))
            return

        # 2) left the desk during focus / came back after a long time
        away = now - self.store.last_person_ts
        if focus == "work" and s["away_warning"] and self.store.last_person_ts and away > float(s["away_after_s"]):
            if not self._away_warned:
                self._away_warned = True
                self.store.bump("away_warnings")
                self._spawn(self.say(self.line("away"), "sad", "double"))
        elif away < 5:
            if s["greet_on_return"] and focus == "off" and self._last["away"] and now - self._last["away"] > 1800:
                self._spawn(self._react(SITUATIONS["welcome"], self.line("welcome"), "happy"))
            self._away_warned = False
            self._last["away"] = now

        # 3) react to emotions (not while the user is in a focus block)
        emo = self.store.stable_emotion()
        if s["emotion_talk"] and focus != "work" and not d["busy"] and emo in REACT_TO \
                and (emo != self._last_reacted_emotion or now - self._last["emotion"] > float(s["emotion_cooldown_s"])) \
                and now - self._last["emotion"] > 60:
            self._last["emotion"] = now
            self._last_reacted_emotion = emo
            self._spawn(self._react(f"the camera shows the user looks {emo}", self.line(f"emo_{emo}")))

        # 4) room climate
        t, h = d.get("temp"), d.get("hum")
        if s["climate_tips"] and t is not None and now - self._last["climate"] > 3600:
            tip = None
            if t >= 32:
                tip = self.line("hot", t=round(t))
            elif h is not None and h >= 75:
                tip = self.line("humid")
            elif h is not None and h <= 25:
                tip = self.line("dry")
            if tip:
                self._last["climate"] = now
                self._spawn(self.say(tip, "surprised"))

    # ------------------------------------------------------------ wake word
    def wake_phrase(self) -> str:
        name = (self.s.get("bot_name") or "Desky").strip()
        return (self.s.get("wake_phrase") or "").strip() or f"Hey {name}"

    def wake_names(self) -> list[str]:
        """Accepted openings: the wake phrase, the name on its own (speech models often drop
        "hey"), and any extra spellings from Settings."""
        name = (self.s.get("bot_name") or "Desky").strip().lower()
        greetings = {"hey", "hi", "hello", "ok", "okay", "yo", "এই", "হেই", "হ্যালো"}
        extra = [w.strip().lower() for w in (self.s.get("wake_words") or "").split(",")
                 if w.strip() and w.strip().lower() not in greetings]
        phrase = self.wake_phrase().lower()
        core = phrase.split()[-1] if phrase.split() else name
        return list(dict.fromkeys([phrase, name, core] + extra))

    @staticmethod
    def _sound(word: str) -> str:
        """Rough sound-alike key: Oly / Olly / Ollie / Oli / Holy -> 'oli'."""
        w = re.sub(r"[^a-z]", "", word.lower())
        if not w:
            return word.lower()                    # Bangla etc.: compare as written
        if len(w) > 2 and w[0] == "h" and w[1] in "aeiou":
            w = w[1:]                              # dropped h: "holy" ~ "oly"
        w = re.sub(r"(ie|ee|ey|y)$", "i", w)
        w = re.sub(r"(.)\1+", r"\1", w)          # double letters
        return w

    def addressed(self, text: str) -> tuple[bool, str]:
        """Was this said to the bot? Like other assistants, the sentence must START with the
        wake phrase ("Hey Oly, ..."). Returns (yes, text without the wake phrase)."""
        import difflib
        words = re.sub(r"[^\w\sঀ-৿']", " ", text.lower()).split()
        if not words:
            return False, ""
        greet = {"hey", "hi", "hello", "ok", "okay", "yo", "এই", "হেই", "হ্যালো"}

        def same(cand: str, name: str) -> bool:
            if cand == name:
                return True
            a, b = self._sound(cand), self._sound(name)
            return a == b or (len(b) >= 4 and difflib.SequenceMatcher(None, a, b).ratio() >= 0.8)

        for name in self.wake_names():
            n = len(name.split())
            # at the start, optionally after a greeting
            start = 1 if words[0] in greet and len(words) > 1 else 0
            if same(" ".join(words[start:start + n]), name) or same(" ".join(words[:n]), name):
                skip = start + n if same(" ".join(words[start:start + n]), name) else n
                return True, " ".join(words[skip:])
        return False, text

    # ------------------------------------------------------------ voice
    async def handle_voice(self, headers, wav: bytes, auto: bool = False):
        """Voice in English or Bangla.

        1. On this PC (about 1 s): is it English or Bangla, and does it start with the wake phrase?
           The wake check uses an English-letters transcript, so "হেই ইমো" reads as "Hey Emo" and
           Bangla chatter that isn't for the bot never leaves the PC.
        2. English: transcribed on this PC, answered as usual (works offline).
           Bangla: one Gemini request hears the audio and answers (needs internet + key).
        Returns None to stay quiet (hands-free, not addressed)."""
        import time as _t
        t0 = _t.time()
        self.store.update_device(headers)
        fixed = self.s.get("language", "auto")
        hot = ", ".join(n for n in self.wake_names()[:1] + [w.strip() for w in (self.s.get("wake_words") or "").split(",")]
                        if n and not re.search(r"[\u0980-\u09FF]", n))
        if auto and not await run_in_threadpool(self.speech.has_speech, wav):
            return None                           # noise, music, a fan: no speech in it
        try:
            # One pass gives the language and, for English, the final text (~0.4 s)
            en_text, p_en, p_bn = await run_in_threadpool(self.speech.transcribe_detect, wav, hot or None,
                                                          80 if auto else None)
            bangla = p_bn > p_en or p_en < 0.4
            # For the wake check Bangla needs an English-letters version ("হেই ইমো" -> "Hey Emo").
            # Often the one pass already gives that; only redo it if it came out in another script.
            if bangla and re.search(r"[^\x00-\x7F]", en_text):
                en_text = await run_in_threadpool(self.speech.transcribe, wav, "en", hot or None)
        except Exception as e:  # noqa: BLE001
            log.exception("STT failed: %s", e)
            p_en, p_bn, en_text, bangla = 1.0, 0.0, "", False
        t_stt = _t.time() - t0

        rest = en_text
        if auto:
            words = re.sub(r"[^\w\u0980-\u09FF]", "", en_text)
            if len(words) < 2 or en_text.strip().lower().strip(".!") in ("thank you", "thanks for watching", "you"):
                return None                       # noise or a typical speech-model hallucination
            yes, rest = self.addressed(en_text)
            if not yes and _t.time() < self._followup_until:
                yes, rest = True, en_text
            if not yes:
                log.info("Heard (not for me) [en %.2f / bn %.2f]: %r", p_en, p_bn, en_text)
                return None
            self._followup_until = 0
            if not rest.strip() or len(rest.split()) == 0:          # just the wake phrase
                reply = "হ্যাঁ, বলো?" if bangla else random.choice(["Yes?", "I'm listening.", "Hmm?"])
                self._followup_until = _t.time() + 10
                audio = await self.speech.tts(reply, self.voice_for(reply)) or silence_wav()
                return audio, reply, en_text, "listening"

        face, actions = None, []
        if bangla:
            history = self.store.history_for_llm()
            res = await run_in_threadpool(self.brain.listen_and_reply, wav, self.context(), history,
                                          self.s["bot_name"], self.s["persona"], fixed)
            if res and res["transcript"]:
                user_text = self.strip_wake(res["transcript"])
                self.last_lang = "bn" if detect_lang(user_text) == "bn" else self.last_lang
                local = offline.match(user_text, detect_lang(user_text), self.offline_context())
                if local and local.always:      # time, room, timer...: exact local answer
                    reply, face, actions = local.text, local.face, local.actions
                else:
                    reply, face, actions = res["reply"], res["face"], res["actions"]
            else:
                user_text = en_text
                reply, face = ("বাংলা বুঝতে আমার ইন্টারনেট আর এআই key লাগবে। ইংরেজিতে বললে এখনই উত্তর দিতে পারি।",
                               "confused")
        else:
            user_text = rest if auto else en_text
            if user_text:
                reply, face, actions = await self.chat_reply(user_text)
            else:
                reply, face = self.line("not_heard"), "confused"

        if user_text:
            self.note_user_language(user_text)
            self.store.add_chat("user", user_text, "voice")
            self.store.bump("voice_chats")
            self.apply_actions(actions)
            self.store.add_chat("assistant", reply, "voice")
            self.store.save()
        t_ai = _t.time() - t0 - t_stt
        audio = await self.speech.tts(reply, self.voice_for(reply)) or silence_wav()
        log.info("Voice [%s]: hearing %.2fs, answer %.2fs, speech %.2fs (total %.2fs)", "bn" if bangla else "en",
                 t_stt, t_ai, _t.time() - t0 - t_stt - t_ai, _t.time() - t0)
        return audio, reply, user_text, face or "happy"

    # Bengali letters -> rough Latin, to compare a Bangla-written name with the bot's name
    _BN2LAT = {**dict(zip("অআইঈউঊঋএঐওঔ", ["o", "a", "i", "i", "u", "u", "ri", "e", "oi", "o", "ou"])),
               **dict(zip("কখগঘঙচছজঝঞটঠডঢণতথদধনপফবভমযরলশষসহড়ঢ়য়ৎংঃ",
                          ["k", "kh", "g", "gh", "ng", "ch", "ch", "j", "jh", "n", "t", "th", "d", "dh", "n",
                           "t", "th", "d", "dh", "n", "p", "f", "b", "bh", "m", "j", "r", "l", "sh", "sh", "s", "h",
                           "r", "r", "y", "t", "ng", "h"])),
               **dict(zip("ািীুূৃেৈোৌ", ["a", "i", "i", "u", "u", "ri", "e", "oi", "o", "ou"])), "্": "", "ঁ": ""}

    @classmethod
    def _bn_key(cls, word: str) -> str:
        lat = "".join(cls._BN2LAT.get(ch, ch) for ch in word.lower())
        lat = re.sub(r"[^a-z]", "", lat)
        return lat.translate(str.maketrans("eo", "iu"))          # e~i, o~u: ইমো ~ ইমু ~ emo

    def strip_wake(self, text: str) -> str:
        """Remove a leading wake phrase from a transcript (English or Bangla script)."""
        words = text.split()
        names = {self._bn_key(w) for n in self.wake_names() for w in n.split()} | {"hi", "hii", "i"}
        greet = {"hey", "hi", "hello", "ok", "okay", "এই", "হেই", "হ্যালো", "ও"}
        cut = 0
        for i, w in enumerate(words[:3]):
            clean = re.sub(r"[^\w\u0980-\u09FF]", "", w).lower()
            if clean in greet or self._bn_key(clean) in names:
                cut = i + 1
        return " ".join(words[cut:]).strip(" ,.।?!") or text

    # ------------------------------------------------------------ text chat
    def offline_context(self) -> offline.Context:
        d, day = self.store.device, self.store.day()
        return offline.Context(
            bot_name=self.s["bot_name"], temp=d.get("temp"), hum=d.get("hum"), focus=d.get("focus", "off"),
            focus_left=int(d.get("focus_left") or 0), work_min=int(self.s.get("work_min", 25)),
            sessions=day["focus_sessions"], focus_minutes=round(day["focus_seconds"] / 60),
            phone_warnings=day["phone_warnings"],
            emotion=self.store.stable_emotion() or (self.store.vision.get("emotion") if self.store.vision.get("face") else None),
            volume=int(d.get("volume") or 70),
            week=report.week_report(self.store))

    async def chat_reply(self, text: str) -> tuple[str, str | None, list[str]]:
        fixed = self.s.get("language", "auto")
        lang = fixed if fixed in ("en", "bn") else detect_lang(text)

        # 1) Commands and facts: answered here, instantly, even without internet
        local = offline.match(text, lang, self.offline_context())
        if local and local.always:
            return local.text, local.face, local.actions

        # 2) Everything else: the AI
        history = self.store.history_for_llm()
        reply, face, actions = await run_in_threadpool(
            self.brain.reply, text, self.context(), history, self.s["bot_name"], self.s["persona"], fixed)
        if reply not in AI_ERROR_TEXTS:
            return reply, face, actions

        # 3) AI unavailable (no key, no internet, busy): built-in small talk
        ans = local or offline.unknown(lang, self.brain.status.get("state", "offline"))
        return ans.text, ans.face, ans.actions

    async def dashboard_chat(self, text: str, speak: bool) -> dict:
        self.note_user_language(text)
        self.store.add_chat("user", text, "dashboard")
        self.store.bump("voice_chats")
        reply, face, actions = await self.chat_reply(text)
        self.apply_actions(actions)
        if speak:
            await self.say(reply, face, source="dashboard")
        else:
            self.store.add_chat("assistant", reply, "dashboard")
            if face:
                self.store.queue_cmd({"t": "face", "v": face, "a": 5000})
        self.store.save()
        return {"reply": reply, "face": face, "actions": actions}
