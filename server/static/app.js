// DeskBot dashboard: tabs, English / Bangla, polls /api/state every second.
const $ = (id) => document.getElementById(id);

// ------------------------------------------------------------------ words
const T = {
  en: {
    tabHome: "Home", tabChat: "Chat", tabFocus: "Focus", tabBot: "Bot", tabSettings: "Settings", tabReports: "Reports",
    rDay: "Day", rWeek: "Week", rFocusTime: "Focus time", rCompleted: "Completed", rMood: "Mood while focusing",
    rSessionsTitle: "Sessions", rToday: "Today", rThisWeek: "This week", rHours: "Focus by hour", rDays: "Focus per day",
    rNoSessions: "No focus sessions yet. Hold the bot's button or press Start on the Focus tab.",
    rNoMood: "No mood data yet.", rDoneTag: "completed", rStopTag: "stopped early", rMin: "min",
    rPhones: (n) => `${n} phone`, rWeekNote: (avg, chg, streak, best) =>
      `Average <b>${avg} min</b> a day` + (chg === null ? "" : `, <b>${chg >= 0 ? "+" : ""}${chg}%</b> vs last week`) +
      (streak > 1 ? `, <b>${streak} day</b> streak` : "") + (best ? `. Best day: <b>${best}</b>.` : "."),
    rDayNote: (n, away) => n ? `${n} session${n === 1 ? "" : "s"}` + (away ? `, left the desk ${away} time${away === 1 ? "" : "s"}.` : ".") : "",
    sWakePhrase: "Wake phrase (start your question with it)",
    online: "Online", offline: "Offline", connecting: "Connecting",
    temp: "Temperature", hum: "Humidity", room2h: "Room over the last 2 hours",
    camWait: "Waiting for camera", noFace: "No face in view", person: "Someone is here", phoneSeen: "Phone spotted",
    sessions: "Focus sessions", minutes: "Minutes focused", phoneWarn: "Phone warnings",
    chatPh: "Message in English or বাংলা", speakAloud: "Read replies out loud on the bot",
    chatEmpty: "Say hello. I understand English and Bangla.",
    chatFail: "No reply from the server. Check that it is running, then send again.",
    work: "Focus", break: "Break", min: "min",
    phase: { off: "Ready", work: "Focus time", break: "Break time", paused: "Paused" },
    expressions: "Expressions", sound: "Sound", moodSounds: "Mood sounds when the face changes",
    beep: "Beep", tune: "Tune", alarm: "Alarm", screen: "Screen", showFace: "Face", showRoom: "Room", showInfo: "Info",
    sayExact: "Say something through the bot", sayPh: "Type the exact words", activity: "Recent activity", nothing: "Nothing has happened yet",
    aiTest: "Test connection", aiTesting: "Testing...",
    sHandsFree: "Hands-free: always listening, answers when called by name",
    sWake: "Extra wake words, comma separated (the bot's name always works)",
    ai: { ok: "connected", no_key: "no API key", offline: "no internet", busy: "busy at Google, try again soon", error: "error", unknown: "not used yet" },
    aiLine: (p, st) => `${p} ${st}`,
    cTitle: "Bot connection", cBot: "Bot", cWifi: "Wi-Fi", cServer: "Server",
    cMove: "Move the bot to another server",
    cMoveHint: "Use the address shown in the other PC's DeskBot window. The bot checks it first and keeps this server if it can't reach it.",
    cWifiTitle: "Change Wi-Fi",
    cWifiHint: "The bot restarts as \"DeskBot-Setup\" (password 11221122). Join it from your phone and pick the new network. If nobody does, it rejoins this Wi-Fi after 5 minutes.",
    cWifiBtn: "Open Wi-Fi setup", cSent: "Sent. The bot checks the address and moves if it answers.",
    cWifiConfirm: "Restart the bot into Wi-Fi setup now? It goes offline until you choose a network (or for 5 minutes).",
    cWifiSent: "The bot is restarting into Wi-Fi setup. Join \"DeskBot-Setup\" (password 11221122) from your phone.",
    cOffline: "The bot is offline, so it can't receive this right now.", cMoved: "The bot moved to the new server.",
    cMoveFailed: "The bot couldn't reach that address and stayed on this server.",
    sBrain: "AI brain", sGeminiKey: "Gemini API key", sGeminiHint: "Get a free key at", sModel: "Model",
    sClaudeKey: "Claude API key", keyEmpty: "Paste your key", keySaved: "Key saved. Paste a new one to replace it",
    sVoice: "Language and voice", sName: "Bot name", sLang: "Bot speaks", langAuto: "Whatever I use",
    sVoiceEn: "English voice", sVoiceBn: "Bangla voice", sPersona: "Personality",
    sBehave: "Behaviour", sMood: "Talk about my mood", sPhone: "Warn me about my phone while focusing",
    sAway: "Remind me if I leave the desk", sClimate: "Tips about the room", sGreet: "Welcome me back",
    advanced: "Fine tuning", sMoodCd: "Seconds between mood comments", sPhoneCd: "Seconds between phone warnings",
    sPhoneHits: "Frames with a phone, out of 5", sPhoneConf: "Phone confidence, 0 to 1", sAwayS: "Away after, seconds",
    sWork: "Default focus, minutes", sBreak: "Default break, minutes", save: "Save settings", saved: "Settings saved",
    face: { neutral: "Calm", happy: "Happy", sad: "Sad", angry: "Grumpy", surprised: "Surprised", sleepy: "Sleepy",
            love: "Love", focused: "Focused", confused: "Confused", warning: "Warning" },
    line: { neutral: "Here with you", happy: "Happy to see you", sad: "Feeling a little blue", angry: "Not amused",
            surprised: "Oh!", sleepy: "Getting sleepy", love: "This is nice", focused: "Focusing with you",
            confused: "Hmm?", warning: "Phone down, please", thinking: "Thinking", listening: "Listening",
            speaking: "Talking", offline: "The bot is offline. Check its power and Wi-Fi." },
    emo: { neutral: "You look calm", happy: "You look happy", sad: "You look sad", angry: "You look upset",
           surprised: "You look surprised", fear: "You look worried", disgust: "You look annoyed", contempt: "You look unimpressed" },
  },
  bn: {
    tabHome: "হোম", tabChat: "চ্যাট", tabFocus: "ফোকাস", tabBot: "বট", tabSettings: "সেটিংস", tabReports: "রিপোর্ট",
    rDay: "দিন", rWeek: "সপ্তাহ", rFocusTime: "ফোকাস সময়", rCompleted: "সম্পূর্ণ", rMood: "পড়ার সময় মেজাজ",
    rSessionsTitle: "সেশন", rToday: "আজ", rThisWeek: "এই সপ্তাহ", rHours: "ঘণ্টা অনুযায়ী ফোকাস", rDays: "দিন অনুযায়ী ফোকাস",
    rNoSessions: "এখনো কোনো ফোকাস সেশন নেই। বটের বাটন চেপে ধরো বা ফোকাস ট্যাবে শুরু চাপো।",
    rNoMood: "এখনো মেজাজের তথ্য নেই।", rDoneTag: "সম্পূর্ণ", rStopTag: "আগে থামানো", rMin: "মিনিট",
    rPhones: (n) => `ফোন ${n}`, rWeekNote: (avg, chg, streak, best) =>
      `দিনে গড়ে <b>${avg} মিনিট</b>` + (chg === null ? "" : `, আগের সপ্তাহের চেয়ে <b>${chg >= 0 ? "+" : ""}${chg}%</b>`) +
      (streak > 1 ? `, টানা <b>${streak} দিন</b>` : "") + (best ? `। সেরা দিন: <b>${best}</b>।` : "।"),
    rDayNote: (n, away) => n ? `${n}টা সেশন` + (away ? `, ডেস্ক ছেড়েছ ${away} বার।` : "।") : "",
    sWakePhrase: "জাগানোর বাক্য (প্রশ্ন এটা দিয়ে শুরু করো)",
    online: "অনলাইন", offline: "অফলাইন", connecting: "সংযোগ হচ্ছে",
    temp: "তাপমাত্রা", hum: "আর্দ্রতা", room2h: "গত ২ ঘণ্টায় ঘরের অবস্থা",
    camWait: "ক্যামেরার অপেক্ষায়", noFace: "মুখ দেখা যাচ্ছে না", person: "কেউ আছে", phoneSeen: "ফোন দেখা গেছে",
    sessions: "ফোকাস সেশন", minutes: "ফোকাসের মিনিট", phoneWarn: "ফোন সতর্কতা",
    chatPh: "বাংলা বা English-এ লেখো", speakAloud: "বট উত্তরগুলো জোরে পড়ে শোনাবে",
    chatEmpty: "হ্যালো বলো। আমি বাংলা আর ইংরেজি দুটোই বুঝি।",
    chatFail: "সার্ভার থেকে উত্তর আসেনি। সার্ভার চালু আছে কিনা দেখে আবার পাঠাও।",
    work: "ফোকাস", break: "বিরতি", min: "মিনিট",
    phase: { off: "প্রস্তুত", work: "ফোকাস টাইম", break: "বিরতি", paused: "থামানো" },
    expressions: "অভিব্যক্তি", sound: "শব্দ", moodSounds: "মুখ বদলালে মেজাজের শব্দ",
    beep: "বিপ", tune: "সুর", alarm: "অ্যালার্ম", screen: "স্ক্রিন", showFace: "মুখ", showRoom: "ঘর", showInfo: "তথ্য",
    sayExact: "বটকে দিয়ে কিছু বলাও", sayPh: "ঠিক যে কথাগুলো বলবে", activity: "সাম্প্রতিক কার্যকলাপ", nothing: "এখনো কিছু ঘটেনি",
    aiTest: "সংযোগ পরীক্ষা করো", aiTesting: "পরীক্ষা হচ্ছে...",
    sHandsFree: "হ্যান্ডস-ফ্রি: সবসময় শুনবে, নাম ধরে ডাকলে উত্তর দেবে",
    sWake: "আরও ডাকনাম, কমা দিয়ে আলাদা করো (বটের নাম সবসময় কাজ করে)",
    ai: { ok: "সংযুক্ত", no_key: "API key নেই", offline: "ইন্টারনেট নেই", busy: "গুগলে ব্যস্ত, একটু পরে চেষ্টা করো", error: "সমস্যা", unknown: "এখনো ব্যবহার হয়নি" },
    aiLine: (p, st) => `${p} ${st}`,
    cTitle: "বটের সংযোগ", cBot: "বট", cWifi: "Wi-Fi", cServer: "সার্ভার",
    cMove: "বটকে অন্য সার্ভারে সরাও",
    cMoveHint: "অন্য পিসির DeskBot উইন্ডোতে দেখানো ঠিকানাটা দাও। বট আগে যাচাই করবে, না পেলে এই সার্ভারেই থাকবে।",
    cWifiTitle: "Wi-Fi বদলাও",
    cWifiHint: "বট \"DeskBot-Setup\" নামে আবার চালু হবে (পাসওয়ার্ড 11221122)। ফোন থেকে যুক্ত হয়ে নতুন নেটওয়ার্ক বেছে নাও। কেউ না করলে ৫ মিনিট পর আগের Wi-Fi-তে ফিরবে।",
    cWifiBtn: "Wi-Fi সেটআপ খোলো", cSent: "পাঠানো হয়েছে। বট ঠিকানাটা যাচাই করে সাড়া পেলে সরে যাবে।",
    cWifiConfirm: "এখনই বটকে Wi-Fi সেটআপে চালু করবে? নেটওয়ার্ক বেছে না নেওয়া পর্যন্ত (বা ৫ মিনিট) বট অফলাইন থাকবে।",
    cWifiSent: "বট Wi-Fi সেটআপে চালু হচ্ছে। ফোন থেকে \"DeskBot-Setup\" (পাসওয়ার্ড 11221122)-এ যুক্ত হও।",
    cOffline: "বট অফলাইন, তাই এখন এটা পাঠানো যাবে না।", cMoved: "বট নতুন সার্ভারে চলে গেছে।",
    cMoveFailed: "বট ঐ ঠিকানায় পৌঁছাতে পারেনি, তাই এই সার্ভারেই আছে।",
    sBrain: "এআই মস্তিষ্ক", sGeminiKey: "Gemini API key", sGeminiHint: "ফ্রি key নাও এখান থেকে", sModel: "মডেল",
    sClaudeKey: "Claude API key", keyEmpty: "তোমার key পেস্ট করো", keySaved: "Key সেভ করা আছে। বদলাতে নতুনটা পেস্ট করো",
    sVoice: "ভাষা ও কণ্ঠ", sName: "বটের নাম", sLang: "বট কথা বলবে", langAuto: "আমি যেটায় বলি",
    sVoiceEn: "ইংরেজি কণ্ঠ", sVoiceBn: "বাংলা কণ্ঠ", sPersona: "স্বভাব",
    sBehave: "আচরণ", sMood: "আমার মেজাজ নিয়ে কথা বলবে", sPhone: "ফোকাসের সময় ফোন ধরলে সতর্ক করবে",
    sAway: "ডেস্ক ছেড়ে গেলে মনে করাবে", sClimate: "ঘর নিয়ে পরামর্শ", sGreet: "ফিরে এলে স্বাগত জানাবে",
    advanced: "সূক্ষ্ম সমন্বয়", sMoodCd: "মেজাজ নিয়ে কথার মাঝে সেকেন্ড", sPhoneCd: "ফোন সতর্কতার মাঝে সেকেন্ড",
    sPhoneHits: "৫টির মধ্যে কত ফ্রেমে ফোন", sPhoneConf: "ফোন নিশ্চয়তা, ০ থেকে ১", sAwayS: "কত সেকেন্ড পর অনুপস্থিত",
    sWork: "ফোকাসের সময়, মিনিট", sBreak: "বিরতির সময়, মিনিট", save: "সেটিংস সেভ করো", saved: "সেটিংস সেভ হয়েছে",
    face: { neutral: "শান্ত", happy: "খুশি", sad: "দুঃখী", angry: "রাগী", surprised: "অবাক", sleepy: "ঘুমঘুম",
            love: "ভালোবাসা", focused: "মনোযোগী", confused: "বিভ্রান্ত", warning: "সতর্ক" },
    line: { neutral: "তোমার সাথেই আছি", happy: "তোমাকে দেখে খুশি", sad: "একটু মন খারাপ", angry: "একদম ভালো লাগছে না",
            surprised: "ওমা!", sleepy: "ঘুম পাচ্ছে", love: "খুব ভালো লাগছে", focused: "একসাথে ফোকাস করছি",
            confused: "হুম?", warning: "ফোনটা রাখো", thinking: "ভাবছি", listening: "শুনছি", speaking: "বলছি",
            offline: "বট অফলাইন। পাওয়ার আর Wi-Fi দেখো।" },
    emo: { neutral: "তোমাকে শান্ত দেখাচ্ছে", happy: "তোমাকে খুশি দেখাচ্ছে", sad: "তোমার মন খারাপ মনে হচ্ছে", angry: "তোমাকে বিরক্ত দেখাচ্ছে",
           surprised: "তুমি অবাক হয়েছ", fear: "তোমাকে চিন্তিত দেখাচ্ছে", disgust: "তোমাকে বিরক্ত দেখাচ্ছে", contempt: "তোমাকে উদাস দেখাচ্ছে" },
  },
};
const FACES = ["neutral", "happy", "sad", "angry", "surprised", "sleepy", "love", "focused", "confused", "warning"];

let lang = "en";
try { lang = localStorage.getItem("deskbot-lang") || "en"; } catch (e) { /* storage blocked */ }
const t = (k) => T[lang][k] ?? T.en[k] ?? k;
const num = (n) => (lang === "bn" ? String(n).replace(/\d/g, (d) => "০১২৩৪৫৬৭৮৯"[d]) : String(n));

// Tiny drawn eyes for each expression (no emoji). 46 x 28 box.
function miniEyes(face) {
  const c = face === "warning" ? "var(--alert)" : "currentColor";
  const pair = (fn) => fn(9) + fn(29);
  const shapes = {
    neutral: (x) => `<rect x="${x}" y="5" width="8" height="18" rx="3"/>`,
    happy: (x) => `<path d="M${x} 18 q4 -9 8 0" fill="none" stroke="${c}" stroke-width="3" stroke-linecap="round"/>`,
    sad: (x) => x < 20 ? `<path d="M${x} 11 L${x + 8} 7 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>` : `<path d="M${x} 7 L${x + 8} 11 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>`,
    angry: (x) => x < 20 ? `<path d="M${x} 7 L${x + 8} 12 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>` : `<path d="M${x} 12 L${x + 8} 7 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>`,
    surprised: (x) => `<circle cx="${x + 4}" cy="14" r="6"/>`,
    sleepy: (x) => `<rect x="${x - 1}" y="17" width="10" height="3" rx="1.5"/>`,
    love: (x) => `<path d="M${x + 4} 22 l-5 -5 a3 3 0 0 1 5 -4 a3 3 0 0 1 5 4z"/>`,
    focused: (x) => `<rect x="${x - 1}" y="10" width="10" height="9" rx="3"/>`,
    confused: (x) => x < 20 ? `<rect x="${x}" y="5" width="8" height="18" rx="3"/>` : `<rect x="${x}" y="11" width="8" height="10" rx="3"/>`,
    warning: (x) => x < 20 ? `<path d="M${x} 7 L${x + 8} 12 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>` : `<path d="M${x} 12 L${x + 8} 7 V22 a2 2 0 0 1 -2 2 h-4 a2 2 0 0 1 -2 -2z"/>`,
  };
  return `<svg viewBox="0 0 46 28" fill="${c}" aria-hidden="true">${pair(shapes[face] || shapes.neutral)}</svg>`;
}

function applyLang() {
  document.documentElement.lang = lang;
  document.body.classList.toggle("bn", lang === "bn");
  document.querySelectorAll("[data-i18n]").forEach((el) => { el.textContent = t(el.dataset.i18n); });
  document.querySelectorAll("[data-i18n-ph]").forEach((el) => { el.placeholder = t(el.dataset.i18nPh); });
  document.querySelectorAll(".lang button").forEach((b) => b.classList.toggle("on", b.dataset.lang === lang));
  $("faces").innerHTML = FACES.map((f) => `<button class="face-btn" data-face="${f}">${miniEyes(f)}<span>${T[lang].face[f]}</span></button>`).join("");
  chatKey = "";
  if (state) { render(state); updateKeyFields(state.settings); }
  if (typeof renderReport === "function" && repData) renderReport();
}
document.querySelectorAll(".lang button").forEach((b) => b.addEventListener("click", () => {
  lang = b.dataset.lang;
  try { localStorage.setItem("deskbot-lang", lang); } catch (e) { /* ignore */ }
  applyLang();
}));

// ------------------------------------------------------------------ tabs
let tab = "home";
try { tab = localStorage.getItem("deskbot-tab") || "home"; } catch (e) { /* ignore */ }
function showTab(name) {
  tab = name;
  document.querySelectorAll(".tabs button").forEach((b) => b.classList.toggle("on", b.dataset.tab === name));
  document.querySelectorAll(".panel").forEach((p) => p.classList.toggle("on", p.id === `tab-${name}`));
  try { localStorage.setItem("deskbot-tab", name); } catch (e) { /* ignore */ }
  if (name === "chat") { const c = $("chat"); c.scrollTop = c.scrollHeight; }
  if (name === "reports" && typeof loadReport === "function") loadReport();
}
document.querySelectorAll(".tabs button").forEach((b) => b.addEventListener("click", () => showTab(b.dataset.tab)));

// ------------------------------------------------------------------ helpers
let state = null, chatKey = "", pending = null, chatError = "", settingsLoaded = false, screenMode = "face";

async function post(url, body) {
  const r = await fetch(url, { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify(body) });
  if (!r.ok) throw new Error(await r.text());
  return r.json();
}
const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;" }[c]));
const clock = (ts) => new Date(ts * 1000).toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
const mmss = (s) => `${String(Math.floor(s / 60)).padStart(2, "0")}:${String(Math.max(0, s) % 60).padStart(2, "0")}`;

// ------------------------------------------------------------------ render
function render(s) {
  const d = s.device, v = s.vision, L = T[lang];
  $("botName").textContent = s.settings.bot_name || "Desky";
  const status = $("botStatus");
  status.textContent = d.online ? L.online : L.offline;
  status.classList.toggle("on", d.online);

  // the screen: live eyes mirror the bot
  const eyes = $("eyes");
  const face = d.face === "speaking" ? "neutral" : (d.face || "neutral");
  eyes.dataset.face = d.online ? face : "sleepy";
  eyes.classList.toggle("off", !d.online);
  $("moodLine").textContent = d.online ? (L.line[d.face] || L.line.neutral) : L.line.offline;
  document.querySelectorAll(".face-btn").forEach((b) => b.classList.toggle("on", b.dataset.face === d.face));

  // camera note
  const emo = s.stable_emotion || (v.face ? v.emotion : null);
  $("camNote").lastElementChild.textContent = !d.camera ? L.camWait : v.face ? (L.emo[emo] || L.person) : v.person ? L.person : L.noFace;
  $("phoneFlag").classList.toggle("hidden", !v.phone);

  $("temp").textContent = d.temp != null ? num(d.temp.toFixed(1)) : "--";
  $("hum").textContent = d.hum != null ? num(Math.round(d.hum)) : "--";
  drawSpark(s.sensor_log);

  // today, shown on Home and Focus
  const sess = num(s.today.focus_sessions), mins = num(Math.round(s.today.focus_seconds / 60)), ph = num(s.today.phone_warnings);
  $("tSessions").textContent = sess; $("tMinutes").textContent = mins; $("tPhone").textContent = ph;
  $("fSessions").textContent = sess; $("fMinutes").textContent = mins; $("fPhone").textContent = ph;

  // focus ring
  const active = d.focus !== "off";
  const frac = active && d.focus_total ? 1 - d.focus_left / d.focus_total : 0;
  $("ringFill").style.strokeDashoffset = (326.7 * (1 - frac)).toFixed(1);
  const ring = document.querySelector(".ring");
  ring.classList.toggle("brk", d.focus === "break");
  ring.classList.toggle("paused", d.focus === "paused");
  $("timer").textContent = num(active ? mmss(d.focus_left) : mmss(($("workMin").value || d.work_min) * 60));
  $("phase").textContent = L.phase[d.focus] || d.focus;
  const running = d.focus === "work" || d.focus === "break";
  const tg = $("btnToggle");
  tg.querySelector("use").setAttribute("href", running ? "#i-pause" : "#i-play");
  tg.setAttribute("aria-label", running ? "Pause" : d.focus === "paused" ? "Resume" : "Start");
  $("btnStop").disabled = !active;

  // chat: re-render only when something changed
  const last = s.chat[s.chat.length - 1];
  const key = `${lang}|${s.chat.length}|${last ? last.ts : 0}|${pending ? pending.ts : 0}|${chatError}`;
  if (key !== chatKey) {
    chatKey = key;
    const box = $("chat");
    const nearBottom = box.scrollHeight - box.scrollTop - box.clientHeight < 60;
    let html = s.chat.map((m) => `<div class="msg ${m.role}">${esc(m.text)}<time>${clock(m.ts)}</time></div>`).join("");
    if (pending) html += `<div class="msg user">${esc(pending.text)}</div><div class="typing"><i></i><i></i><i></i></div>`;
    if (chatError) html += `<div class="msg error">${esc(chatError)}</div>`;
    box.innerHTML = html || `<div class="chat-empty"><span class="who-mark"><i></i><i></i></span>${L.chatEmpty}</div>`;
    if (nearBottom || pending) box.scrollTop = box.scrollHeight;
  }

  $("feed").innerHTML = s.events.map((e) =>
    `<li><time>${clock(e.ts)}</time><span class="${e.kind}">${esc(e.text)}</span></li>`).join("") || `<li class="muted">${L.nothing}</li>`;

  // AI status (header + settings)
  const ai = s.ai || {};
  const sum = ai.summary || "unknown";
  const prov = (ai.provider || s.settings.ai_provider || "gemini").replace(/^./, (c) => c.toUpperCase());
  const aiText = L.aiLine(prov, (L.ai || T.en.ai)[sum] || sum);
  const aiEl = $("aiStatus");
  aiEl.textContent = aiText;
  aiEl.className = `who-status ai ${sum === "ok" ? "on" : sum === "unknown" ? "" : "bad"}`;
  if (!window.aiTesting) {
    const st = $("aiState");
    st.textContent = aiText + (sum === "ok" && ai.ms ? ` (${(ai.ms / 1000).toFixed(1)} s)` : "");
    st.className = `ai-state ${sum === "ok" ? "on" : sum === "unknown" ? "" : "bad"}`;
  }

  // bot connection
  const ipEl = $("cBotIp");
  ipEl.textContent = d.ip || "-";
  if (d.ip) ipEl.href = `http://${d.ip}`;
  const sig = d.rssi == null ? "" : d.rssi > -60 ? " (strong)" : d.rssi > -72 ? " (good)" : " (weak)";
  $("cWifi").textContent = d.online && d.wifi ? d.wifi + sig : "-";
  $("cServer").textContent = (s.server_url || "").replace("http://", "");
  const ev = s.events.find((e) => e.kind === "device" && (e.text === "server_change_failed" || e.text === "server_changed"));
  if (ev && window.connPending && ev.ts > window.connSentAt) {
    $("cStatus").textContent = ev.text === "server_changed" ? t("cMoved") : t("cMoveFailed");
    window.connPending = false;
  } else if (window.connPending && !d.online) {
    // a successful move reports to the NEW server, so here the bot simply goes quiet
    $("cStatus").textContent = t("cMoved");
    window.connPending = false;
  }

  $("moodSounds").checked = !!s.settings.mood_sounds;
  $("wakePhraseInput").placeholder = `Hey ${s.settings.bot_name || "Oly"}`;
  if (!settingsLoaded) {
    settingsLoaded = true;
    $("volume").value = d.volume; $("volVal").textContent = num(d.volume);
    $("workMin").value = s.settings.work_min; $("breakMin").value = s.settings.break_min;
    fillSettings(s.settings);
  }
}

// Fixed-size SVG sparkline: it can never change the page width.
function drawSpark(log) {
  const svg = $("spark");
  if (!log || log.length < 2) { svg.innerHTML = ""; return; }
  const t0 = log[0][0], t1 = log[log.length - 1][0] || t0 + 1;
  const path = (idx) => {
    const vals = log.map((r) => r[idx]);
    let lo = Math.min(...vals), hi = Math.max(...vals);
    if (hi - lo < 2) { lo -= 1; hi += 1; }
    return log.map((r, i) => `${i ? "L" : "M"}${(((r[0] - t0) / (t1 - t0)) * 296 + 2).toFixed(1)},${(60 - ((r[idx] - lo) / (hi - lo)) * 56).toFixed(1)}`).join("");
  };
  svg.innerHTML =
    `<path d="${path(2)}" fill="none" stroke="var(--line)" stroke-width="2" vector-effect="non-scaling-stroke"/>` +
    `<path d="${path(1)}" fill="none" stroke="var(--eye)" stroke-width="2" vector-effect="non-scaling-stroke"/>`;
}

function setProvider(p) {
  $("settingsForm").elements.ai_provider.value = p;
  document.querySelectorAll("#providerSeg button").forEach((b) => b.classList.toggle("on", b.dataset.provider === p));
  document.querySelectorAll(".prov").forEach((el) => el.classList.toggle("hidden", el.dataset.for !== p));
}
document.querySelectorAll("#providerSeg button").forEach((b) => b.addEventListener("click", () => setProvider(b.dataset.provider)));

function updateKeyFields(st) {
  document.querySelectorAll("[data-key-field]").forEach((el) => {
    el.placeholder = st[`${el.name}_set`] ? t("keySaved") : t("keyEmpty");
  });
}

function fillSettings(st) {
  const f = $("settingsForm");
  for (const [k, val] of Object.entries(st)) {
    const el = f.elements[k];
    if (!el || el.dataset.keyField !== undefined) continue;
    if (el.type === "checkbox") el.checked = !!val;
    else {
      if (el.tagName === "SELECT" && ![...el.options].some((o) => o.value === val)) el.add(new Option(val, val));
      el.value = val;
    }
  }
  setProvider(st.ai_provider || "gemini");
  updateKeyFields(st);
}

// ------------------------------------------------------------------ actions
function focusCmd(action) {
  post("/api/focus", { action, work: +$("workMin").value, brk: +$("breakMin").value }).catch((e) => alert(e.message));
}
// One button: start when off, pause while running, resume when paused.
function focusToggle() {
  const f = state ? state.device.focus : "off";
  focusCmd(f === "off" ? "start" : f === "paused" ? "resume" : "pause");
  // show the change right away; the next poll confirms it
  if (state) {
    state.device.focus = f === "off" ? "work" : f === "paused" ? "work" : "paused";
    if (f === "off") { state.device.focus_total = +$("workMin").value * 60; state.device.focus_left = state.device.focus_total; }
    render(state);
  }
}

$("faces").addEventListener("click", (e) => {
  const b = e.target.closest(".face-btn");
  if (b) post("/api/face", { face: b.dataset.face, seconds: 6 });
});
$("volume").addEventListener("input", (e) => { $("volVal").textContent = num(e.target.value); });
$("volume").addEventListener("change", (e) => post("/api/volume", { value: +e.target.value }));
$("moodSounds").addEventListener("change", (e) => post("/api/settings", { mood_sounds: e.target.checked }));

document.querySelectorAll("#screenSeg button").forEach((b) => b.addEventListener("click", () => {
  screenMode = b.dataset.mode;
  document.querySelectorAll("#screenSeg button").forEach((x) => x.classList.toggle("on", x === b));
  post("/api/screen", { mode: screenMode });
}));
document.querySelector(`#screenSeg button[data-mode="face"]`).classList.add("on");

$("chatForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const input = $("chatInput"), text = input.value.trim();
  if (!text || pending) return;
  input.value = "";
  chatError = "";
  pending = { text, ts: Date.now() };
  if (state) render(state);
  const ctl = new AbortController();
  const timer = setTimeout(() => ctl.abort(), 45000);       // never leave the dots spinning
  try {
    const r = await fetch("/api/chat", { method: "POST", headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ text, speak: $("speak").checked }), signal: ctl.signal });
    if (!r.ok) throw new Error(String(r.status));
  } catch (err) {
    chatError = t("chatFail");
    input.value = text;                                      // keep the message so it can be resent
  } finally { clearTimeout(timer); pending = null; await refresh(); input.focus(); }
});

$("sayForm").addEventListener("submit", (e) => {
  e.preventDefault();
  const text = $("sayInput").value.trim();
  if (!text) return;
  $("sayInput").value = "";
  post("/api/say", { text }).catch((err) => alert(err.message));
});

$("settingsForm").addEventListener("submit", async (e) => {
  e.preventDefault();
  const body = {};
  for (const el of e.target.elements) {
    if (!el.name) continue;
    body[el.name] = el.type === "checkbox" ? el.checked : el.type === "number" ? +el.value : el.value;
  }
  try {
    const st = await post("/api/settings", body);
    document.querySelectorAll("[data-key-field]").forEach((el) => { el.value = ""; });
    fillSettings(st);
    $("saved").textContent = t("saved");
    setTimeout(() => ($("saved").textContent = ""), 2500);
  } catch (err) { alert(err.message); }
});

$("aiTestBtn").addEventListener("click", async () => {
  window.aiTesting = true;
  $("aiState").textContent = t("aiTesting");
  $("aiState").className = "ai-state";
  try { await post("/api/ai/test", {}); } catch (e) { /* status shows the result */ }
  window.aiTesting = false;
  await refresh();
});

$("cMoveBtn").addEventListener("click", async () => {
  const url = $("cNewServer").value.trim();
  if (!url) { $("cNewServer").focus(); return; }
  if (!state || !state.device.online) { $("cStatus").textContent = t("cOffline"); return; }
  try {
    await post("/api/bot/server", { url });
    window.connPending = true;
    window.connSentAt = Date.now() / 1000 - 1;
    $("cStatus").textContent = t("cSent");
  } catch (err) { $("cStatus").textContent = err.message; }
});
$("cWifiBtn").addEventListener("click", async () => {
  if (!state || !state.device.online) { $("cStatus").textContent = t("cOffline"); return; }
  if (!confirm(t("cWifiConfirm"))) return;
  try { await post("/api/bot/wifi-setup", {}); $("cStatus").textContent = t("cWifiSent"); }
  catch (err) { $("cStatus").textContent = err.message; }
});

// ------------------------------------------------------------------ reports
const MOOD_COLORS = { happy: "#3ccf7a", neutral: "#9aa5b4", sad: "#4c8dff", angry: "#f2655a", surprised: "#ffc83d",
                      fear: "#b56cff", disgust: "#7fb28f", contempt: "#b0a089" };
let repRange = "day", repOffset = 0, repData = null;

// Local date (not UTC): in UTC+6 "today" must not turn into yesterday before 6 AM
function isoDate(d) {
  return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, "0")}-${String(d.getDate()).padStart(2, "0")}`;
}
function repDate() {
  const d = new Date();
  d.setDate(d.getDate() - repOffset * (repRange === "week" ? 7 : 1));
  return d;
}
function fmtMin(m) {
  m = Math.round(m || 0);
  const h = Math.floor(m / 60), r = m % 60;
  return num(h ? `${h}h ${String(r).padStart(2, "0")}m` : `${r}m`);
}
function dayName(iso, long) {
  const d = new Date(iso + "T12:00:00");
  return d.toLocaleDateString(lang === "bn" ? "bn-BD" : undefined, long ? { weekday: "long", day: "numeric", month: "short" } : { weekday: "short" });
}

async function loadReport() {
  const d = repDate();
  try {
    const r = await fetch(`/api/report?range=${repRange}&date=${isoDate(d)}`);
    repData = await r.json();
    renderReport();
  } catch (e) { /* server offline: keep the last report */ }
}

function renderReport() {
  const r = repData, L = T[lang];
  if (!r) return;
  document.querySelectorAll("#repRange button").forEach((b) => b.classList.toggle("on", b.dataset.range === repRange));
  $("repNext").disabled = repOffset === 0;
  $("repLabel").textContent = repRange === "day"
    ? (repOffset === 0 ? L.rToday : dayName(r.date, true))
    : (repOffset === 0 ? L.rThisWeek : `${dayName(r.start, true)} – ${dayName(r.end, true)}`);

  $("rFocus").textContent = fmtMin(r.focus_minutes);
  $("rSessions").textContent = num(r.sessions);
  $("rPhone").textContent = num(r.phone_warnings);
  $("rDone").textContent = r.completion == null ? "-" : num(r.completion) + "%";
  $("rNote").innerHTML = repRange === "week"
    ? L.rWeekNote(num(r.avg_minutes), r.change_pct == null ? null : num(r.change_pct), num(r.streak), r.best_day ? dayName(r.best_day) : null)
    : (r.session_list.length ? L.rDayNote(num(r.session_list.length), r.away_warnings ? num(r.away_warnings) : 0) : "");

  // bar chart: 7 days for a week, hours 6-24 for a day
  const svg = $("rChart");
  let values, labels, todayIdx = -1;
  if (repRange === "week") {
    values = r.days.map((d) => d.focus_minutes);
    labels = r.days.map((d) => dayName(d.date));
    todayIdx = repOffset === 0 ? 6 : -1;
    $("rChartTitle").textContent = L.rDays;
  } else {
    values = r.hours.slice(6);
    labels = values.map((_, i) => ((i + 6) % 3 === 0 ? num(i + 6) : ""));
    $("rChartTitle").textContent = L.rHours;
  }
  const max = Math.max(...values, repRange === "week" ? 30 : 10);
  const n = values.length, gap = repRange === "week" ? 18 : 6, bw = (700 - gap * (n + 1)) / n;
  let html = [0.5, 1].map((f) => `<line class="grid" x1="0" x2="700" y1="${170 - f * 150}" y2="${170 - f * 150}"/>`).join("");
  html += `<text x="4" y="${170 - 150 - 4}">${fmtMin(max)}</text>`;
  values.forEach((v, i) => {
    const h = v ? Math.max(4, (v / max) * 150) : 2;
    const cls = i === todayIdx ? "bar today" : v ? "bar" : "bar dim";
    html += `<rect class="${cls}" x="${gap + i * (bw + gap)}" y="${170 - h}" width="${bw}" height="${h}" rx="${Math.min(6, bw / 2)}"><title>${labels[i] || ""} ${fmtMin(v)}</title></rect>`;
  });
  svg.innerHTML = html;
  const axis = $("rAxis");
  axis.style.gridTemplateColumns = `repeat(${n}, 1fr)`;
  axis.innerHTML = labels.map((l) => `<span>${l}</span>`).join("");

  // mood strip
  const moods = Object.entries(r.moods || {});
  const tot = moods.reduce((a, [, v]) => a + v, 0);
  $("rMoodBar").innerHTML = moods.map(([k, v]) => `<i style="width:${(v / tot) * 100}%;background:${MOOD_COLORS[k] || "#999"}"></i>`).join("");
  $("rMoodLegend").innerHTML = tot ? moods.map(([k, v]) =>
    `<span style="--c:${MOOD_COLORS[k] || "#999"}">${(L.face[k] || L.emo[k] || k).replace(/^You look /, "")} ${num(Math.round((v / tot) * 100))}%</span>`).join("")
    : `<span class="rep-empty">${L.rNoMood}</span>`;

  // sessions
  const list = (r.session_list || []).slice().reverse();
  $("rList").innerHTML = list.length ? list.map((x) => {
    const t1 = new Date(x.start * 1000), t2 = x.end ? new Date(x.end * 1000) : null;
    const tm = (d) => d.toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
    const when = (repRange === "week" ? dayName(x.date) + " " : "") + tm(t1) + (t2 ? "–" + tm(t2) : "");
    const meta = [fmtMin(x.focus_minutes), x.phone_warnings ? L.rPhones(num(x.phone_warnings)) : "", x.mood ? (L.face[x.mood] || x.mood) : ""].filter(Boolean).join(", ");
    const tag = x.end == null ? "" : `<span class="tag ${x.completed ? "" : "stopped"}">${x.completed ? L.rDoneTag : L.rStopTag}</span>`;
    return `<li><time>${num(when)}</time><span class="meta">${meta}</span>${tag}</li>`;
  }).join("") : `<li class="rep-empty">${L.rNoSessions}</li>`;
}

document.querySelectorAll("#repRange button").forEach((b) => b.addEventListener("click", () => {
  repRange = b.dataset.range; repOffset = 0; loadReport();
}));
$("repPrev").addEventListener("click", () => { repOffset++; loadReport(); });
$("repNext").addEventListener("click", () => { if (repOffset > 0) { repOffset--; loadReport(); } });
setInterval(() => { if (tab === "reports" && !document.hidden) loadReport(); }, 30000);

// ------------------------------------------------------------------ loops
async function refresh() {
  try {
    const s = await (await fetch("/api/state")).json();
    state = s;
    render(s);
  } catch (e) {
    $("botStatus").textContent = t("offline");
    $("botStatus").classList.remove("on");
  }
}

function refreshCam() {
  if (tab !== "home" || document.hidden) { setTimeout(refreshCam, 1000); return; }
  const next = new Image();
  next.onload = () => { $("cam").src = next.src; setTimeout(refreshCam, 250); };
  next.onerror = () => setTimeout(refreshCam, 2500);
  next.src = `/api/frame.jpg?t=${Date.now()}`;
}

$("botStatus").textContent = t("connecting");
applyLang();
showTab(tab);
refresh();
setInterval(refresh, 1000);
refreshCam();
