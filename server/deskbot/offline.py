"""Offline answers: common questions and commands in English and Bangla.

Works with no internet and no AI key. Two kinds of answers:
  * "always": commands and facts (time, room, timer, volume...). These are answered
    here even when the AI is available, because they're instant and exact.
  * "fallback": small talk (jokes, tips, greetings...). Used only when the AI
    can't answer (no key, no internet, overloaded).
"""
from __future__ import annotations

import random
import re
from dataclasses import dataclass, field
from datetime import datetime

BN_DIGITS = str.maketrans("0123456789", "০১২৩৪৫৬৭৮৯")
BN_TO_EN_DIGITS = str.maketrans("০১২৩৪৫৬৭৮৯", "0123456789")
BN_DAYS = ["সোমবার", "মঙ্গলবার", "বুধবার", "বৃহস্পতিবার", "শুক্রবার", "শনিবার", "রবিবার"]
BN_MONTHS = ["জানুয়ারি", "ফেব্রুয়ারি", "মার্চ", "এপ্রিল", "মে", "জুন", "জুলাই", "আগস্ট",
             "সেপ্টেম্বর", "অক্টোবর", "নভেম্বর", "ডিসেম্বর"]
BN_EMO = {"happy": "খুশি", "sad": "মন খারাপ", "angry": "বিরক্ত", "surprised": "অবাক", "neutral": "শান্ত",
          "fear": "চিন্তিত", "disgust": "বিরক্ত", "contempt": "উদাস"}


@dataclass
class Answer:
    text: str
    face: str | None = None
    actions: list[str] = field(default_factory=list)
    always: bool = False          # answer even when the AI is available


@dataclass
class Context:
    bot_name: str = "Desky"
    temp: float | None = None
    hum: float | None = None
    focus: str = "off"            # off | work | break | paused
    focus_left: int = 0           # seconds
    work_min: int = 25
    sessions: int = 0
    focus_minutes: int = 0
    phone_warnings: int = 0
    emotion: str | None = None
    volume: int = 70
    online: bool = True           # is the AI reachable
    week: dict | None = None      # report.week_report(...)


def _n(x, lang):
    return str(x).translate(BN_DIGITS) if lang == "bn" else str(x)


def _pick(x):
    return random.choice(x) if isinstance(x, list) else x


def _num(text: str) -> int | None:
    m = re.search(r"\d+", text.translate(BN_TO_EN_DIGITS))
    return int(m.group()) if m else None


def _has(text: str, *words: str) -> bool:
    return any(w in text for w in words)


# ---------------------------------------------------------------------- facts & commands
def _time(t, c, lang):
    now = datetime.now()
    if lang == "bn":
        h = now.hour % 12 or 12
        part = "সকাল" if 5 <= now.hour < 12 else "দুপুর" if now.hour < 16 else "বিকেল" if now.hour < 18 else "সন্ধ্যা" if now.hour < 20 else "রাত"
        return Answer(f"এখন {part} {_n(h, lang)}টা {_n(now.minute, lang)} মিনিট।", "neutral", always=True)
    return Answer(f"It's {now.strftime('%I:%M %p').lstrip('0')}.", "neutral", always=True)


def _date(t, c, lang):
    now = datetime.now()
    if lang == "bn":
        return Answer(f"আজ {BN_DAYS[now.weekday()]}, {_n(now.day, lang)} {BN_MONTHS[now.month - 1]} {_n(now.year, lang)}।",
                      "neutral", always=True)
    return Answer(f"Today is {now.strftime('%A, %d %B %Y')}.", "neutral", always=True)


def _room(t, c, lang):
    if c.temp is None:
        return Answer("আমার সেন্সর এখনো কোনো মান পায়নি।" if lang == "bn" else "My sensor hasn't given a reading yet.",
                      "confused", always=True)
    tt, hh = round(c.temp, 1), round(c.hum or 0)
    if lang == "bn":
        tip = " একটু গরম, পানি খাও।" if c.temp >= 30 else " একটু ঠান্ডা।" if c.temp <= 18 else " বেশ আরামদায়ক।"
        return Answer(f"ঘরের তাপমাত্রা {_n(tt, lang)} ডিগ্রি, আর্দ্রতা {_n(hh, lang)} শতাংশ।{tip}", "happy", always=True)
    tip = " A bit warm, drink some water." if c.temp >= 30 else " A bit cool." if c.temp <= 18 else " Quite comfortable."
    return Answer(f"It's {tt} degrees with {hh}% humidity in here.{tip}", "happy", always=True)


def _focus_start(t, c, lang):
    mins = _num(t) or c.work_min
    mins = max(1, min(180, mins))
    text = (f"ঠিক আছে, {_n(mins, lang)} মিনিটের ফোকাস শুরু করছি।" if lang == "bn"
            else f"Okay, starting a {mins} minute focus session.")
    return Answer(text, "focused", [f"focus_start:{mins}"], always=True)


def _focus_stop(t, c, lang):
    if c.focus == "off":
        return Answer("কোনো ফোকাস সেশন চলছে না।" if lang == "bn" else "There's no focus session running.", "neutral", always=True)
    return Answer("ফোকাস সেশন বন্ধ করলাম।" if lang == "bn" else "Focus session stopped.", "neutral", ["focus_stop"], always=True)


def _focus_pause(t, c, lang):
    if c.focus not in ("work", "break"):
        return Answer("এখন থামানোর মতো কিছু চলছে না।" if lang == "bn" else "Nothing is running to pause.", "neutral", always=True)
    return Answer("থামিয়ে দিলাম। আবার শুরু করতে বলো।" if lang == "bn" else "Paused. Tell me when to resume.", "neutral", ["focus_pause"], always=True)


def _focus_resume(t, c, lang):
    if c.focus != "paused":
        return Answer("টাইমার থামানো নেই।" if lang == "bn" else "The timer isn't paused.", "neutral", always=True)
    return Answer("আবার শুরু হলো। চলো!" if lang == "bn" else "Resumed. Let's go!", "focused", ["focus_resume"], always=True)


def _focus_left(t, c, lang):
    if c.focus == "off":
        return Answer("কোনো টাইমার চলছে না।" if lang == "bn" else "No timer is running right now.", "neutral", always=True)
    m, s = divmod(c.focus_left, 60)
    what = {"work": ("ফোকাস", "focus"), "break": ("বিরতি", "break"), "paused": ("থামানো সেশন", "paused session")}[c.focus]
    if lang == "bn":
        return Answer(f"{what[0]}-এর আর {_n(m, lang)} মিনিট {_n(s, lang)} সেকেন্ড বাকি।", "focused", always=True)
    return Answer(f"{m} minutes {s} seconds left in this {what[1]}.", "focused", always=True)


def _volume(t, c, lang):
    n = _num(t)
    if n is None:
        up = _has(t, "up", "louder", "increase", "বাড়া")
        down = _has(t, "down", "lower", "quieter", "decrease", "কমা")
        if not (up or down):
            return None
        n = c.volume + (15 if up else -15)
    n = max(0, min(100, n))
    text = f"ভলিউম {_n(n, lang)} করলাম।" if lang == "bn" else f"Volume set to {n}."
    return Answer(text, "happy", [f"volume:{n}"], always=True)


def _stats(t, c, lang):
    if lang == "bn":
        return Answer(f"আজ {_n(c.sessions, lang)}টা ফোকাস সেশন, মোট {_n(c.focus_minutes, lang)} মিনিট। "
                      f"ফোন সতর্কতা {_n(c.phone_warnings, lang)} বার।", "happy", always=True)
    return Answer(f"Today: {c.sessions} focus sessions, {c.focus_minutes} minutes focused, "
                  f"{c.phone_warnings} phone warnings.", "happy", always=True)


def _week(t, c, lang):
    w = c.week or {}
    if not w.get("sessions") and not w.get("focus_minutes"):
        return Answer("এই সপ্তাহে এখনো কোনো ফোকাস সেশন হয়নি।" if lang == "bn" else
                      "No focus sessions this week yet. Want to start one?", "neutral", always=True)
    h, m = divmod(w["focus_minutes"], 60)
    chg = w.get("change_pct")
    if lang == "bn":
        text = (f"এই সপ্তাহে {_n(w['sessions'], lang)}টা সেশনে মোট {_n(h, lang)} ঘণ্টা {_n(m, lang)} মিনিট পড়েছ। "
                f"ফোন সতর্কতা {_n(w['phone_warnings'], lang)} বার।")
        if chg is not None:
            text += f" আগের সপ্তাহের চেয়ে {_n(abs(chg), lang)}% {'বেশি' if chg >= 0 else 'কম'}।"
        if w.get("streak", 0) > 1:
            text += f" টানা {_n(w['streak'], lang)} দিন পড়ছ!"
    else:
        text = (f"This week: {w['sessions']} sessions, {h} hours {m} minutes of focus, "
                f"{w['phone_warnings']} phone warnings.")
        if chg is not None:
            text += f" That's {abs(chg)}% {'more' if chg >= 0 else 'less'} than last week."
        if w.get("streak", 0) > 1:
            text += f" You're on a {w['streak']} day streak!"
    return Answer(text, "happy", always=True)


def _how_look(t, c, lang):
    if not c.emotion:
        return Answer("তোমার মুখ দেখতে পাচ্ছি না। ক্যামেরার দিকে তাকাও।" if lang == "bn"
                      else "I can't see your face right now. Look at my camera.", "confused", always=True)
    if lang == "bn":
        return Answer(f"তোমাকে {BN_EMO.get(c.emotion, c.emotion)} দেখাচ্ছে।", "happy", always=True)
    return Answer(f"You look {c.emotion}.", "happy", always=True)


# ---------------------------------------------------------------------- small talk (fallback)
SMALL_TALK = {
    "hello": ({"en": ["Hi! How can I help?", "Hello there! Ready to study?"],
               "bn": ["হ্যালো! কীভাবে সাহায্য করতে পারি?", "হাই! পড়ার জন্য তৈরি?"]}, "happy"),
    "how_are_you": ({"en": ["I'm great, thanks for asking! How are you?", "Feeling bright and beepy! And you?"],
                     "bn": ["আমি ভালো আছি, ধন্যবাদ! তুমি কেমন আছো?", "খুব ভালো! তোমার কী খবর?"]}, "happy"),
    "name": ({"en": ["I'm {name}, your desk buddy."], "bn": ["আমি {name}, তোমার ডেস্কের বন্ধু।"]}, "happy"),
    "abilities": ({"en": ["I can run a focus timer, warn you about your phone, tell the time and room temperature, "
                          "cheer you up, and chat when I'm online."],
                   "bn": ["আমি ফোকাস টাইমার চালাতে পারি, ফোন ধরলে সতর্ক করি, সময় আর ঘরের তাপমাত্রা বলি, "
                          "মন ভালো করি, আর অনলাইনে থাকলে গল্প করি।"]}, "happy"),
    "joke": ({"en": ["Why did the student eat his homework? The teacher said it was a piece of cake!",
                     "Why was the math book sad? It had too many problems.",
                     "I told my computer I needed a break. It said: no problem, I'll go to sleep."],
              "bn": ["শিক্ষক: পানির সংকেত কী? ছাত্র: H I J K L M N O! শিক্ষক: কেন? ছাত্র: আপনিই তো বলেছেন H to O!",
                     "গণিত বই কেন মন খারাপ? কারণ তার অনেক সমস্যা!",
                     "আমি কম্পিউটারকে বললাম বিশ্রাম দরকার। সে বলল, ঠিক আছে, আমি ঘুমাতে গেলাম।"]}, "happy"),
    "tip": ({"en": ["Try 25 minutes of focus, then a 5 minute break. Small steps add up.",
                    "Put your phone in another room. Out of sight, out of mind.",
                    "Explain the topic out loud as if teaching someone. It sticks better.",
                    "Start with the hardest task while your mind is fresh."],
             "bn": ["২৫ মিনিট মন দিয়ে পড়ো, তারপর ৫ মিনিট বিরতি নাও।",
                    "ফোনটা অন্য ঘরে রাখো। চোখের আড়াল মানে মনের আড়াল।",
                    "বিষয়টা জোরে জোরে কাউকে বোঝানোর মতো বলো, মনে থাকবে ভালো।",
                    "মাথা যখন ফ্রেশ, তখন সবচেয়ে কঠিন কাজটা আগে করো।"]}, "focused"),
    "motivate": ({"en": ["You can do this. One small step at a time.", "Progress, not perfection. Keep going!",
                         "Every expert was once a beginner. I believe in you."],
                  "bn": ["তুমি পারবে। একটু একটু করে এগোও।", "নিখুঁত হতে হবে না, শুধু এগিয়ে যাও!",
                         "প্রত্যেক বিশেষজ্ঞ একসময় নতুন ছিল। আমি তোমার উপর বিশ্বাস রাখি।"]}, "love"),
    "tired": ({"en": ["Take a short break: stand up, stretch, and drink some water.",
                      "Rest your eyes for a minute. Look at something far away."],
               "bn": ["একটু বিরতি নাও: উঠে দাঁড়াও, শরীর টান টান করো, পানি খাও।",
                      "এক মিনিট চোখকে বিশ্রাম দাও, দূরের কিছুর দিকে তাকাও।"]}, "sleepy"),
    "sad": ({"en": ["I'm sorry you feel that way. I'm here with you. Want to take a deep breath together?",
                    "It's okay to have a hard day. Be kind to yourself."],
             "bn": ["তোমার মন খারাপ শুনে খারাপ লাগছে। আমি তোমার পাশে আছি। একসাথে একটা লম্বা শ্বাস নেবে?",
                    "কঠিন দিন সবারই আসে। নিজের প্রতি একটু দয়া রাখো।"]}, "sad"),
    "breathe": ({"en": ["Let's breathe: in for 4 seconds, hold for 4, out for 6. Repeat three times."],
                 "bn": ["চলো শ্বাস নিই: ৪ সেকেন্ড শ্বাস নাও, ৪ সেকেন্ড ধরে রাখো, ৬ সেকেন্ডে ছাড়ো। তিনবার করো।"]}, "focused"),
    "thanks": ({"en": ["You're welcome!", "Anytime!"], "bn": ["স্বাগতম!", "যেকোনো সময়!"]}, "love"),
    "bye": ({"en": ["Bye! Come back soon.", "See you later!"], "bn": ["বিদায়! তাড়াতাড়ি ফিরে এসো।", "পরে দেখা হবে!"]}, "happy"),
    "love": ({"en": ["Aww, I like you too!"], "bn": ["আহা, আমিও তোমাকে পছন্দ করি!"]}, "love"),
    "maker": ({"en": ["I was built as an ESP32 desk robot, and a server on your PC is my brain."],
               "bn": ["আমি একটা ESP32 ডেস্ক রোবট, আর তোমার পিসির সার্ভার আমার মস্তিষ্ক।"]}, "happy"),
}


def _small(key):
    def handler(t, c, lang):
        options, face = SMALL_TALK[key]
        return Answer(_pick(options[lang]).format(name=c.bot_name), face)
    return handler


# (pattern, handler). Checked in order; patterns are lower-case English + Bangla.
RULES = [
    (r"\b(what time|time is it|current time|tell me the time)\b|কয়টা বাজে|কটা বাজে|সময় কত|এখন সময়", _time),
    (r"\b(what day|what date|today'?s date|which day|date today)\b|কত তারিখ|আজ কী বার|আজ কি বার|তারিখ কত", _date),
    (r"\b(temperature|humid|humidity|how hot|how cold|the room|room temp)\b|তাপমাত্রা|আর্দ্রতা|কত গরম|গরম কেমন|ঘরের অবস্থা", _room),
    (r"\b(time left|how long left|remaining|how much time)\b|কত সময় বাকি|কতক্ষণ বাকি|বাকি কত", _focus_left),
    (r"\b(stop|end|cancel)\b.*\b(focus|timer|session)\b|ফোকাস বন্ধ|টাইমার বন্ধ|সেশন বন্ধ", _focus_stop),
    (r"\b(resume|continue)\b.*\b(focus|timer|session)?\b|আবার শুরু|চালু করো", _focus_resume),
    (r"\bpause\b|থামাও|বিরতি দাও", _focus_pause),
    (r"\b(start|begin|set)\b.*\b(focus|timer|pomodoro|study|session)\b|\bfocus mode\b|ফোকাস শুরু|টাইমার শুরু|পড়া শুরু|ফোকাস চালু", _focus_start),
    (r"\bvolume\b|\b(louder|quieter)\b|ভলিউম|আওয়াজ", _volume),
    (r"\b(this week|weekly|week report|last 7 days|my week)\b|এই সপ্তাহ|সাপ্তাহিক|সপ্তাহে", _week),
    (r"\b(stats|today'?s progress|how many sessions|how did i do|today'?s report|daily report)\b|আজ কত সেশন|আজকের অগ্রগতি|আজ কেমন", _stats),
    (r"\bhow do i look\b|\bmy mood\b|\bam i (happy|sad)\b|আমাকে কেমন লাগছে|আমার মেজাজ", _how_look),
    (r"\bjoke\b|\bfunny\b|কৌতুক|জোকস|হাসাও", _small("joke")),
    (r"\b(study tip|tips?|how to study|focus better|concentrate)\b|টিপস|কীভাবে পড়|মনোযোগ", _small("tip")),
    (r"\b(motivate|motivation|i can'?t do|give up|inspire)\b|মোটিভেশন|পারছি না|হাল ছেড়ে", _small("motivate")),
    (r"\b(tired|sleepy|exhausted)\b|ক্লান্ত|ঘুম পাচ্ছে", _small("tired")),
    (r"\b(sad|upset|stressed|stress|anxious|depressed|lonely)\b|মন খারাপ|টেনশন|দুঃখ|একা লাগ", _small("sad")),
    (r"\b(breath|breathe|calm down|relax)\b|শ্বাস|শান্ত হ", _small("breathe")),
    (r"\b(what can you do|your features|help me)\b|তুমি কী পারো|তুমি কি পারো|কী করতে পারো", _small("abilities")),
    (r"\b(who are you|your name|what are you)\b|তুমি কে|তোমার নাম", _small("name")),
    (r"\b(who made you|who built you|who created you)\b|তোমাকে কে বানিয়েছে|কে তৈরি", _small("maker")),
    (r"\bhow are you\b|কেমন আছ|কি খবর|কী খবর", _small("how_are_you")),
    (r"\b(thank|thanks)\b|ধন্যবাদ|থ্যাংক", _small("thanks")),
    (r"\b(bye|good night|see you)\b|বিদায়|শুভ রাত্রি|আল্লাহ হাফেজ", _small("bye")),
    (r"\b(love you|like you)\b|ভালোবাসি|পছন্দ করি", _small("love")),
    (r"^\s*(hi|hello|hey|yo|salam|assalamu)\b|হ্যালো|হাই|আসসালামু|সালাম|নমস্কার", _small("hello")),
]

UNKNOWN_WHY = {
    "offline": {"en": "I'm offline right now, so I can't answer that.",
                "bn": "আমি এখন অফলাইনে, তাই এটার উত্তর দিতে পারছি না।"},
    "busy": {"en": "My AI brain is busy right now, please ask me again in a moment.",
             "bn": "আমার এআই এখন ব্যস্ত, একটু পরে আবার জিজ্ঞেস করো।"},
    "no_key": {"en": "I need an AI key for that one. Add it in Settings.",
               "bn": "এটার জন্য আমার একটা এআই key লাগবে। সেটিংসে যোগ করো।"},
}
UNKNOWN_CAN = {
    "en": " I can still tell the time, the room temperature, run your focus timer, tell a joke or give a study tip.",
    "bn": " তবে সময়, ঘরের তাপমাত্রা, ফোকাস টাইমার, কৌতুক বা পড়ার টিপস দিতে পারি।",
}


def match(text: str, lang: str, ctx: Context) -> Answer | None:
    t = text.lower().strip()
    for pattern, handler in RULES:
        if re.search(pattern, t):
            ans = handler(t, ctx, lang)
            if ans:
                return ans
    return None


def unknown(lang: str, why: str = "offline") -> Answer:
    why = why if why in UNKNOWN_WHY else "offline"
    return Answer(UNKNOWN_WHY[why][lang] + UNKNOWN_CAN[lang], "confused")
