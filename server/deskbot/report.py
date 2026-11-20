"""Day and week reports built from the saved focus sessions and daily stats."""
from __future__ import annotations

from collections import Counter
from datetime import date, datetime, timedelta


def _day_stats(store, d: str) -> dict:
    s = store.stats.get(d) or {}
    return {
        "date": d,
        "focus_minutes": round(s.get("focus_seconds", 0) / 60),
        "sessions": s.get("focus_sessions", 0),
        "phone_warnings": s.get("phone_warnings", 0),
        "away_warnings": s.get("away_warnings", 0),
        "voice_chats": s.get("voice_chats", 0),
        "emotions": s.get("emotions", {}),
    }


def _session_view(x: dict) -> dict:
    moods = x.get("moods", {})
    top = max(moods, key=moods.get) if moods else None
    return {
        "start": x["start"], "end": x.get("end"), "date": x["date"],
        "planned_min": x.get("planned_min", 0),
        "focus_minutes": round(x.get("focused_s", 0) / 60, 1),
        "phone_warnings": x.get("phone_warnings", 0),
        "away_warnings": x.get("away_warnings", 0),
        "completed": bool(x.get("completed")),
        "mood": top,
    }


def _moods(day_list: list[dict]) -> dict:
    total = Counter()
    for d in day_list:
        total.update({k: v for k, v in d["emotions"].items()})
    return {k: round(v) for k, v in total.most_common()}


def day_report(store, day: str | None = None) -> dict:
    day = day or date.today().isoformat()
    d = _day_stats(store, day)
    sessions = [_session_view(x) for x in store.all_sessions() if x["date"] == day]
    done = sum(1 for x in sessions if x["completed"])
    return {
        "range": "day", "date": day, **{k: v for k, v in d.items() if k not in ("date", "emotions")},
        "moods": _moods([d]),
        "completion": round(done / len(sessions) * 100) if sessions else None,
        "session_list": sessions,
        "hours": [  # focus minutes per hour, for a small chart
            round(sum(x["focus_minutes"] for x in sessions
                      if datetime.fromtimestamp(x["start"]).hour == h), 1) for h in range(24)],
    }


def week_report(store, end: str | None = None) -> dict:
    end_d = date.fromisoformat(end) if end else date.today()
    days = [_day_stats(store, (end_d - timedelta(days=6 - i)).isoformat()) for i in range(7)]
    prev = [_day_stats(store, (end_d - timedelta(days=13 - i)).isoformat()) for i in range(7)]
    total = sum(d["focus_minutes"] for d in days)
    prev_total = sum(d["focus_minutes"] for d in prev)
    first = days[0]["date"]
    sessions = [_session_view(x) for x in store.all_sessions() if first <= x["date"] <= end_d.isoformat()]
    done = sum(1 for x in sessions if x["completed"])

    # streak: consecutive days with at least one focus session, ending today/end
    streak, cur = 0, end_d
    while _day_stats(store, cur.isoformat())["sessions"] > 0 or (
            cur == end_d and _day_stats(store, cur.isoformat())["focus_minutes"] > 0):
        streak += 1
        cur -= timedelta(days=1)
        if streak > 365:
            break

    best = max(days, key=lambda d: d["focus_minutes"])
    return {
        "range": "week", "start": first, "end": end_d.isoformat(),
        "focus_minutes": total,
        "sessions": sum(d["sessions"] for d in days),
        "phone_warnings": sum(d["phone_warnings"] for d in days),
        "away_warnings": sum(d["away_warnings"] for d in days),
        "voice_chats": sum(d["voice_chats"] for d in days),
        "avg_minutes": round(total / 7),
        "change_pct": round((total - prev_total) / prev_total * 100) if prev_total else None,
        "best_day": best["date"] if best["focus_minutes"] else None,
        "streak": streak,
        "completion": round(done / len(sessions) * 100) if sessions else None,
        "moods": _moods(days),
        "days": [{k: d[k] for k in ("date", "focus_minutes", "sessions", "phone_warnings")} for d in days],
        "session_list": sessions[-30:],
    }
