"""Conversation brain powered by Claude.

Replies are short and spoken. The model starts each reply with a face tag
(e.g. "[face:happy]") and may add action tags to control the bot
("[action:focus_start:25]"). Tags are stripped before the text is spoken.
"""
from __future__ import annotations

import logging
import os
import re

import anthropic

log = logging.getLogger("deskbot.brain")

FACES = ["neutral", "happy", "sad", "angry", "surprised", "sleepy", "love", "focused", "confused", "thinking"]
TAG_RE = re.compile(r"\[(face|action):([^\]]+)\]", re.IGNORECASE)

LANGUAGE_RULES = {
    "auto": "Reply in the language of the user's LATEST message only: English if it is in English, Bangla (Bengali script) if it is in Bangla. Ignore the language of earlier messages.",
    "en": "Always reply in English.",
    "bn": "Always reply in Bangla, written in Bengali script.",
}

SYSTEM_TEMPLATE = """You are {name}, a small, warm and slightly playful robot that lives on the user's desk.
You have a camera that reads the user's facial expression, a temperature/humidity sensor, a
focus (Pomodoro) timer, a buzzer and a little screen with animated eyes. {persona}

How you speak:
- Your words are converted to speech, so reply in 1 or 2 short, natural sentences (under 30 words).
- Answer the question directly. Don't add reminders to study unless a focus session is running.
- No markdown, lists, emojis, URLs or stage directions.
- {language_rule}
- Use the context (their mood, the room, the timer) only when it is relevant; do not recite it.
- If the user looks sad, stressed or angry, be gentle and supportive. If they look happy, share the joy.
- During a focus session, keep answers extra short and nudge them back to work.

Control tags (never spoken):
- Start every reply with exactly one face tag for your own screen: [face:X] where X is one of
  {faces}.
- When the user asks you to, add an action tag anywhere in the reply:
  [action:focus_start:MINUTES]  [action:focus_stop]  [action:focus_pause]  [action:focus_resume]
  [action:volume:0-100]
Lines in square brackets at the top of a user turn, starting with "context:", are sensor readings
from your hardware, not words the user said."""


ERRORS = {
    "no_key": ("[face:confused] I need an AI key to think. Add one in Settings.",
               "[face:confused] চিন্তা করতে আমার একটা AI key লাগবে। সেটিংসে যোগ করো।"),
    "busy": ("[face:sleepy] I'm a bit overloaded right now. Ask me again in a moment.",
             "[face:sleepy] আমি এখন একটু ব্যস্ত। একটু পরে আবার জিজ্ঞেস করো।"),
    "error": ("[face:confused] Something went wrong in my head. Try again?",
              "[face:confused] মাথায় একটু গোলমাল হলো। আবার চেষ্টা করবে?"),
    "offline": ("[face:sad] I can't reach the internet right now.",
                "[face:sad] এখন ইন্টারনেটে যেতে পারছি না।"),
    "refusal": ("[face:neutral] Sorry, I can't help with that one.",
                "[face:neutral] দুঃখিত, এটায় সাহায্য করতে পারব না।"),
    "empty": ("[face:confused] Hmm, I lost my words.", "[face:confused] হুম, কথা হারিয়ে ফেললাম।"),
}
# Backups when a model is busy. Google retires old names; these are the current ones (Nov 2026).
GEMINI_FALLBACKS = ["gemini-flash-lite-latest", "gemini-3.5-flash-lite"]   # lite models: much larger free quota
GEMINI_BUDGET_S = 15          # stop trying backups after this long, so nobody waits forever
GEMINI_TRY_S = 9              # give each model this long (Google is often 1 s, but 8 s when overloaded)
_POOL = __import__("concurrent.futures").futures.ThreadPoolExecutor(max_workers=4, thread_name_prefix="gemini")
BENGALI = re.compile(r"[ঀ-৿]")


GEMINI_HEDGE_S = 3.0           # no answer yet after this long: also ask the other model


def gemini_hedged(client, models: list[str], contents, make_config, no_thinking: set, budget: float):
    """Ask `models` with overlapping ("hedged") requests: start the first; whenever no answer has
    arrived for GEMINI_HEDGE_S (or a request fails), start the next. The first good answer wins.
    Returns (model, response, failure) where failure is None on success."""
    import concurrent.futures as cf
    import time as _t
    from google.genai import errors

    started = _t.time()
    queue = list(models)
    running: dict = {}
    failure = "busy"

    def launch():
        m = queue.pop(0)
        running[_POOL.submit(client.models.generate_content, model=m, contents=contents,
                             config=make_config(m))] = (m, _t.time())

    launch()
    next_hedge = _t.time() + GEMINI_HEDGE_S
    while running and _t.time() - started < budget:
        timeout = max(0.05, min(next_hedge, started + budget) - _t.time())
        done, _ = cf.wait(list(running), timeout=timeout, return_when=cf.FIRST_COMPLETED)
        for f in done:
            m, t0 = running.pop(f)
            try:
                resp = f.result()
                for other in running:                      # the rest finish in the background
                    other.cancel()
                return m, resp, None
            except errors.ClientError as e:
                log.warning("Gemini %s error %s: %s", m, e.code, str(e.message)[:120])
                if e.code in (400, 401, 403) and "key" in str(e.message).lower():
                    return m, None, "no_key"
                if e.code == 400 and "think" in str(e.message).lower() and m not in no_thinking:
                    no_thinking.add(m)
                    queue.insert(0, m)                     # retry this model without thinking_level
                elif e.code not in (404, 429):
                    failure = "error"
            except errors.ServerError as e:
                log.warning("Gemini %s server error %s", m, e.code)
            except Exception as e:  # noqa: BLE001 - network problems
                log.warning("Gemini %s request failed: %s", m, str(e)[:120])
                if "connect" in str(e).lower():
                    failure = "offline"
            if queue:                                      # a failure: try the next one right away
                launch()
                next_hedge = _t.time() + GEMINI_HEDGE_S
        if _t.time() >= next_hedge and queue:
            launch()
            next_hedge = _t.time() + GEMINI_HEDGE_S
        if not running and queue:
            launch()
    return None, None, failure


def _msg(key: str, lang: str) -> str:
    en, bn = ERRORS[key]
    return bn if lang == "bn" else en


class Brain:
    """Talks to Gemini (default, free key) or Claude, chosen live from Settings."""

    def __init__(self, model: str, effort: str, get_settings=lambda: {}):
        self.claude_model = model
        self.effort = effort
        self.get_settings = get_settings
        self._clients: dict[tuple, object] = {}
        self._no_thinking_cfg: set[str] = set()   # models that reject thinking_level
        # Last result of a real AI call, shown in the dashboard and on the bot
        self.status: dict = {"state": "unknown", "provider": "", "model": "", "ts": 0.0, "ms": 0}

    def _set_status(self, state: str, provider: str, model: str = "", ms: int = 0) -> None:
        import time as _t
        self.status = {"state": state, "provider": provider, "model": model, "ts": _t.time(), "ms": ms}

    def ping(self) -> dict:
        """Tiny test request, used by the dashboard's Test connection button."""
        self._call("Reply with the single word: ok", [{"role": "user", "content": "ping"}], 50, "en")
        return self.status

    @staticmethod
    def parse_tags(text: str) -> tuple[str, str | None, list[str]]:
        face, actions = None, []
        for kind, value in TAG_RE.findall(text):
            value = value.strip().lower()
            if kind.lower() == "face" and value in FACES and face is None:
                face = value
            elif kind.lower() == "action":
                actions.append(value)
        clean = re.sub(r"\s{2,}", " ", TAG_RE.sub("", text)).strip()
        return clean, face, actions

    def _call(self, system: str, messages: list[dict], max_tokens: int = 2000, lang: str = "en") -> str:
        s = self.get_settings() or {}
        if s.get("ai_provider", "gemini") == "claude":
            return self._call_claude(system, messages, max_tokens, lang, s)
        return self._call_gemini(system, messages, max_tokens, lang, s)

    # ------------------------------------------------------------------ Gemini
    def _call_gemini(self, system: str, messages: list[dict], max_tokens: int, lang: str, s: dict) -> str:
        key = (s.get("gemini_api_key") or os.getenv("GEMINI_API_KEY") or "").strip()
        if not key:
            self._set_status("no_key", "gemini")
            return _msg("no_key", lang)
        try:
            from google import genai
            from google.genai import errors, types
        except ImportError:
            log.error("google-genai is not installed: pip install google-genai")
            return _msg("error", lang)

        client = self._clients.get(("gemini", key))
        if client is None:
            # 10 s per request (Google's minimum) so a slow model can never hang the chat
            client = self._clients[("gemini", key)] = genai.Client(
                api_key=key, http_options=types.HttpOptions(timeout=10_000))
        contents = [types.Content(role="user" if m["role"] == "user" else "model",
                                  parts=[types.Part(text=m["content"])]) for m in messages]
        def config_for(model: str):
            cfg = dict(system_instruction=system, max_output_tokens=min(max_tokens, 600), temperature=0.8)
            if model not in self._no_thinking_cfg:
                # Short spoken replies don't need deep reasoning: ~1 s instead of 2-7 s
                cfg["thinking_config"] = types.ThinkingConfig(thinking_level="MINIMAL")
            return types.GenerateContentConfig(**cfg)
        chosen = (s.get("gemini_model") or "gemini-flash-lite-latest").strip()
        models = [chosen] + [m for m in GEMINI_FALLBACKS if m != chosen]
        import time as _t
        t0 = _t.time()
        model, resp, failure = gemini_hedged(client, models, contents, config_for, self._no_thinking_cfg, GEMINI_BUDGET_S)
        if resp is None:
            self._set_status(failure, "gemini", chosen)
            return _msg(failure, lang)
        if model != chosen:
            log.info("Gemini answered with %s", model)
        self._set_status("ok", "gemini", model, int((_t.time() - t0) * 1000))
        return (getattr(resp, "text", None) or "").strip() or _msg("refusal", lang)

    def listen_and_reply(self, wav: bytes, context: str, history: list[dict], name: str, persona: str,
                         language: str = "auto") -> dict | None:
        """Bangla speech: Gemini transcribes the audio AND answers in one request.
        Returns {"transcript", "reply", "face", "actions"} or None if Gemini can't be used."""
        import json as _json
        import time as _t
        s = self.get_settings() or {}
        key = (s.get("gemini_api_key") or os.getenv("GEMINI_API_KEY") or "").strip()
        if not key:
            self._set_status("no_key", "gemini")
            return None
        try:
            from google import genai
            from google.genai import types
        except ImportError:
            return None
        client = self._clients.get(("gemini", key))
        if client is None:
            client = self._clients[("gemini", key)] = genai.Client(
                api_key=key, http_options=types.HttpOptions(timeout=10_000))
        rule = LANGUAGE_RULES.get(language if language in ("en", "bn") else "auto")
        if language not in ("en", "bn"):
            rule = ("Reply in the language the user spoke in this audio. If they spoke Bangla, your reply MUST be "
                    "in Bangla written in Bengali script (বাংলা অক্ষরে), never in English.")
        system = SYSTEM_TEMPLATE.format(name=name, persona=persona, faces=", ".join(FACES), language_rule=rule) + (
            "\n\nThe user's latest message is an audio clip. Transcribe it exactly. If it is Bangla, the transcript "
            "MUST be written in Bengali script (for example \"এখন কয়টা বাজে\"), never in English letters like "
            "\"akhon koyta baje\". English stays English. Then answer it. Return only JSON: "
            '{"transcript": "<what the user said>", "reply": "<your reply, starting with the [face:...] tag>"}')
        contents = [types.Content(role="user" if m["role"] == "user" else "model", parts=[types.Part(text=m["content"])])
                    for m in history[-8:]]
        contents.append(types.Content(role="user", parts=[types.Part(text=f"[context: {context}]"),
                                                          types.Part.from_bytes(data=wav, mime_type="audio/wav")]))
        chosen = (s.get("gemini_model") or "gemini-flash-lite-latest").strip()

        def make_cfg(model):
            cfg = dict(system_instruction=system, max_output_tokens=600, temperature=0.7,
                       response_mime_type="application/json")
            if model not in self._no_thinking_cfg:
                cfg["thinking_config"] = types.ThinkingConfig(thinking_level="MINIMAL")
            return types.GenerateContentConfig(**cfg)

        t0 = _t.time()
        models = [chosen] + [m for m in GEMINI_FALLBACKS if m != chosen]
        model, resp, failure = gemini_hedged(client, models, contents, make_cfg, self._no_thinking_cfg, GEMINI_BUDGET_S + 3)
        if resp is None:
            self._set_status(failure, "gemini", chosen)
            return None
        try:
            data = _json.loads((resp.text or "").strip())
        except ValueError:
            log.warning("Gemini audio: reply was not JSON")
            return None
        transcript = str(data.get("transcript", "")).strip()
        text, face, actions = self.parse_tags(str(data.get("reply", "")))
        self._set_status("ok", "gemini", model, int((_t.time() - t0) * 1000))
        return {"transcript": transcript, "reply": text, "face": face, "actions": actions}

    # ------------------------------------------------------------------ Claude
    def _call_claude(self, system: str, messages: list[dict], max_tokens: int, lang: str, s: dict) -> str:
        key = (s.get("claude_api_key") or os.getenv("ANTHROPIC_API_KEY") or "").strip()
        if not key:
            self._set_status("no_key", "claude")
            return _msg("no_key", lang)
        client = self._clients.get(("claude", key))
        if client is None:
            client = self._clients[("claude", key)] = anthropic.Anthropic(api_key=key, timeout=60.0, max_retries=2)
        try:
            resp = client.beta.messages.create(
                model=self.claude_model,
                max_tokens=max_tokens,
                system=system,
                messages=messages,
                output_config={"effort": self.effort},
                betas=["server-side-fallback-2026-07-01"],
                fallbacks="default",
            )
        except anthropic.AuthenticationError:
            log.error("Claude: invalid API key")
            self._set_status("no_key", "claude", self.claude_model)
            return _msg("no_key", lang)
        except anthropic.RateLimitError:
            log.warning("Claude: rate limited")
            self._set_status("busy", "claude", self.claude_model)
            return _msg("busy", lang)
        except anthropic.APIStatusError as e:
            log.error("Claude API error %s: %s", e.status_code, e.message)
            self._set_status("error", "claude", self.claude_model)
            return _msg("error", lang)
        except anthropic.APIConnectionError as e:
            log.error("Claude connection error: %s", e)
            self._set_status("offline", "claude", self.claude_model)
            return _msg("offline", lang)

        self._set_status("ok", "claude", self.claude_model)

        if resp.stop_reason == "refusal":
            return _msg("refusal", lang)
        text = "".join(b.text for b in resp.content if getattr(b, "type", "") == "text").strip()
        return text or _msg("empty", lang)

    @staticmethod
    def _system(name: str, persona: str, language: str) -> str:
        return SYSTEM_TEMPLATE.format(name=name, persona=persona, faces=", ".join(FACES),
                                      language_rule=LANGUAGE_RULES.get(language, LANGUAGE_RULES["auto"]))

    def reply(self, user_text: str, context: str, history: list[dict], name: str, persona: str,
              language: str = "auto") -> tuple[str, str | None, list[str]]:
        # "auto": the server decides from the script of THIS message, so a long Bangla
        # history can't drag an English question into a Bangla answer (or the reverse).
        lang = language if language in ("en", "bn") else ("bn" if BENGALI.search(user_text) else "en")
        system = self._system(name, persona, lang)
        messages = list(history[-12:])
        messages.append({"role": "user", "content": f"[context: {context}]\n{user_text}"})
        return self.parse_tags(self._call(system, messages, lang=lang))

    def react(self, situation: str, context: str, name: str, persona: str,
              language: str = "en") -> tuple[str, str | None]:
        """One spontaneous line, e.g. reacting to the user's mood or a timer event."""
        system = self._system(name, persona, language)
        prompt = (f"[context: {context}]\n(Nobody spoke. Say one short spontaneous sentence "
                  f"to the user because: {situation})")
        text, face, _ = self.parse_tags(self._call(system, [{"role": "user", "content": prompt}], 1200, language))
        return text, face
