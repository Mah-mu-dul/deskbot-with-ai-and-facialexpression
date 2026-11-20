#include "wifi_setup.h"
#include "config.h"
#include "display.h"
#include "buzzer.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <ArduinoJson.h>

// Optional compile-time defaults (used only until something is saved in the portal).
#if __has_include("secrets.h")
  #include "secrets.h"
#endif
#ifndef WIFI_SSID
  #define WIFI_SSID ""
#endif
#ifndef WIFI_PASSWORD
  #define WIFI_PASSWORD ""
#endif
#ifndef SERVER_URL
  #define SERVER_URL ""
#endif

namespace {
const char* NS = "deskbot";

String s_ssid, s_pass, s_server, s_reason;

void load() {
  Preferences p;
  p.begin(NS, false);
  bool nd  = p.getBool("nodefaults", false);       // after a factory reset: start from scratch
  s_ssid   = p.isKey("wifi_ssid") ? p.getString("wifi_ssid", "") : String(nd ? "" : WIFI_SSID);
  s_pass   = p.isKey("wifi_pass") ? p.getString("wifi_pass", "") : String(nd ? "" : WIFI_PASSWORD);
  s_server = p.isKey("server")    ? p.getString("server", "")    : String(nd ? "" : SERVER_URL);
  p.end();
  if (s_ssid == "YourWiFi") s_ssid = "";          // placeholder from secrets.example.h
}

// "192.168.0.111" -> "http://192.168.0.111:8000"
String normaliseServer(String u) {
  u.trim();
  if (!u.length()) return u;
  if (u.indexOf("://") < 0) u = "http://" + u;
  while (u.endsWith("/")) u.remove(u.length() - 1);
  int hostStart = u.indexOf("://") + 3;
  if (u.indexOf(':', hostStart) < 0) u += ":8000";
  return u;
}

const char PAGE_HEAD[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>DeskBot setup</title><style>
:root{--bg:#f3f4f6;--card:#fff;--ink:#24272d;--mut:#6b717c;--line:#e3e6ea;--eye:#3e9bff;--scr:#161b23;--bad:#f2655a}
@media(prefers-color-scheme:dark){:root{--bg:#1c1f24;--card:#24282f;--ink:#eceef1;--mut:#9aa1ad;--line:#323740;--eye:#5aacff;--scr:#12161c}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 system-ui,-apple-system,"Segoe UI",sans-serif}
main{max-width:440px;margin:0 auto;padding:24px 18px 40px}
.face{background:var(--scr);border-radius:22px;padding:26px 0 18px;display:flex;flex-direction:column;align-items:center;gap:12px;margin-bottom:22px}
.eyes{display:flex;gap:26px}.eyes i{width:34px;height:42px;border-radius:12px;background:var(--eye);box-shadow:0 0 18px rgba(62,155,255,.45)}
.face p{margin:0;color:#9fb3c9;font-size:14px}
h1{font-size:20px;margin:0 0 4px}.sub{color:var(--mut);margin:0 0 18px;font-size:14px}
.note{border:1px solid var(--bad);color:var(--bad);border-radius:12px;padding:10px 12px;font-size:14px;margin-bottom:16px}
label{display:block;font-size:13px;color:var(--mut);margin:14px 0 6px}
input{width:100%;padding:12px 14px;border:1px solid var(--line);border-radius:12px;background:var(--card);color:var(--ink);font:inherit}
input:focus{outline:none;border-color:var(--eye)}
.nets{background:var(--card);border:1px solid var(--line);border-radius:14px;overflow:hidden}
.nets button{display:flex;justify-content:space-between;align-items:center;width:100%;padding:11px 14px;border:0;border-top:1px solid var(--line);background:none;color:var(--ink);font:inherit;text-align:left;cursor:pointer}
.nets button:first-child{border-top:0}.nets button.on{background:rgba(62,155,255,.12)}
.bars{display:inline-flex;gap:2px;align-items:flex-end;height:14px}.bars b{width:3px;background:var(--line);border-radius:1px}.bars b.f{background:var(--ink)}
.row{display:flex;gap:8px}.row input{flex:1}
.ghost{border:1px solid var(--line);background:var(--card);color:var(--ink);border-radius:12px;padding:0 14px;font:inherit;cursor:pointer}
.save{margin-top:22px;width:100%;padding:14px;border:0;border-radius:99px;background:var(--eye);color:#fff;font:600 16px system-ui,sans-serif;cursor:pointer}
.hint{font-size:12.5px;color:var(--mut);margin:6px 0 0}.link{color:var(--eye);background:none;border:0;font:inherit;cursor:pointer;padding:0}
</style></head><body><main>
<div class="face"><div class="eyes"><i></i><i></i></div><p>Hi! Let's get me online.</p></div>
)HTML";

const char PAGE_FORM[] PROGMEM = R"HTML(
<h1>Connect DeskBot</h1><p class="sub">Choose your Wi-Fi and tell me where the DeskBot server runs.</p>
%NOTE%
<form method="post" action="/save">
<label>Wi-Fi network <button type="button" class="link" onclick="scan()" style="float:right">Scan again</button></label>
<div class="nets" id="nets"><button type="button" disabled>Scanning…</button></div>
<label for="ssid">Network name</label><input id="ssid" name="ssid" value="%SSID%" required autocomplete="off">
<label for="pass">Password</label>
<div class="row"><input id="pass" name="pass" type="password" value="%PASS%" autocomplete="off"><button type="button" class="ghost" onclick="pass.type=pass.type=='password'?'text':'password'">Show</button></div>
<label for="server">DeskBot server (the PC's IP address)</label>
<input id="server" name="server" value="%SERVER%" placeholder="192.168.0.111" inputmode="url" autocomplete="off" required>
<p class="hint">Shown in the DeskBot window on the PC as "Bot server URL". An IP like 192.168.0.111 is enough.</p>
<button class="save" type="submit">Save and connect</button>
</form>
<script>
function bars(r){const n=r>-55?4:r>-67?3:r>-78?2:1;return '<span class="bars">'+[6,9,11,14].map((h,i)=>'<b style="height:'+h+'px" class="'+(i<n?'f':'')+'"></b>').join('')+'</span>'}
function pick(b,s){document.querySelectorAll('.nets button').forEach(x=>x.classList.remove('on'));b.classList.add('on');ssid.value=s;pass.focus()}
function scan(){nets.innerHTML='<button type="button" disabled>Scanning…</button>';
fetch('/scan').then(r=>r.json()).then(l=>{nets.innerHTML=l.length?'':'<button type="button" disabled>No networks found</button>';
l.forEach(n=>{const b=document.createElement('button');b.type='button';b.innerHTML='<span></span>'+bars(n.rssi);b.firstChild.textContent=n.ssid+(n.open?' (open)':'');
b.onclick=()=>pick(b,n.ssid);if(n.ssid===ssid.value)b.classList.add('on');nets.appendChild(b)})}).catch(()=>{nets.innerHTML='<button type="button" disabled>Scan failed</button>'})}
scan();
</script>
)HTML";

const char PAGE_DONE[] PROGMEM = R"HTML(
<h1>Saved</h1><p class="sub">DeskBot is restarting and joining <b>%SSID%</b>.</p>
<p>Switch your phone back to your normal Wi-Fi. When the bot's screen shows its face with three green dots, it's ready.</p>
<p class="hint">If it can't join, it opens DeskBot-Setup again so you can fix the password.</p>
)HTML";

const char PAGE_TAIL[] PROGMEM = "</main></body></html>";

String htmlEscape(const String& s) {
  String o;
  for (char c : s) {
    if (c == '&') o += "&amp;"; else if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;";
    else if (c == '"') o += "&quot;"; else o += c;
  }
  return o;
}

String scanJson() {
  int n = WiFi.scanNetworks(false, false);
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  // strongest first, no duplicates or hidden networks
  for (int pass = 0; pass < n && arr.size() < 15; pass++) {
    int best = -1;
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i).isEmpty()) continue;
      bool used = false;
      for (JsonObject o : arr) if (WiFi.SSID(i) == (const char*)o["ssid"]) { used = true; break; }
      if (used) continue;
      if (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best)) best = i;
    }
    if (best < 0) break;
    JsonObject o = arr.add<JsonObject>();
    o["ssid"] = WiFi.SSID(best);
    o["rssi"] = WiFi.RSSI(best);
    o["open"] = WiFi.encryptionType(best) == WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  String out;
  serializeJson(doc, out);
  return out;
}
}  // namespace

bool WifiSetup::hasCredentials() {
  load();
  return s_ssid.length() > 0;
}

String WifiSetup::savedSsid() { load(); return s_ssid; }
String WifiSetup::normaliseServer(const String& url) { return ::normaliseServer(url); }

bool WifiSetup::connectSaved(uint32_t timeoutMs) {
  load();
  if (!s_ssid.length()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setHostname(MDNS_NAME);
  WiFi.begin(s_ssid.c_str(), s_pass.c_str());
  Display::bootMessage("Connecting", s_ssid.c_str());
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

void WifiSetup::requestPortalOnNextBoot() {
  Preferences p;
  p.begin(NS, false);
  p.putBool("portal", true);
  p.end();
}

bool WifiSetup::portalRequested() {
  Preferences p;
  p.begin(NS, false);
  bool r = p.getBool("portal", false);
  if (r) p.remove("portal");
  p.end();
  return r;
}

void WifiSetup::runPortal(const char* reason, uint32_t timeoutMs) {
  load();
  s_reason = reason ? reason : "";
  Serial.printf("[setup] portal: %s\n", s_reason.c_str());

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP_STA);                 // AP for the phone, STA so we can scan
  WiFi.softAP(SETUP_AP_NAME, SETUP_AP_PASSWORD);
  delay(200);
  IPAddress ip = WiFi.softAPIP();

  DNSServer dns;
  dns.start(53, "*", ip);                 // every name -> the bot = page pops up by itself
  WebServer server(80);
  bool saved = false;

  auto page = [&](const String& body) {
    String html = FPSTR(PAGE_HEAD);
    html += body;
    html += FPSTR(PAGE_TAIL);
    server.send(200, "text/html", html);
  };
  auto form = [&]() {
    String body = FPSTR(PAGE_FORM);
    String note = s_reason.length() ? "<div class=\"note\">" + htmlEscape(s_reason) + "</div>" : "";
    String server_ip = s_server;
    server_ip.replace("http://", "");
    if (server_ip.endsWith(":8000")) server_ip.remove(server_ip.length() - 5);
    body.replace("%NOTE%", note);
    body.replace("%SSID%", htmlEscape(s_ssid));
    body.replace("%PASS%", htmlEscape(s_pass));
    body.replace("%SERVER%", htmlEscape(server_ip));
    page(body);
  };
  auto toPortal = [&]() {
    server.sendHeader("Location", String("http://") + ip.toString() + "/", true);
    server.send(302, "text/plain", "");
  };

  server.on("/", HTTP_GET, form);
  server.on("/scan", HTTP_GET, [&]() { server.send(200, "application/json", scanJson()); });
  server.on("/save", HTTP_POST, [&]() {
    String ssid = server.arg("ssid"); ssid.trim();
    if (!ssid.length()) { s_reason = "Please choose a Wi-Fi network."; form(); return; }
    String srvIn = server.arg("server"); srvIn.trim();
    if (!srvIn.length()) {
      s_ssid = ssid; s_pass = server.arg("pass");
      s_reason = "Please enter the PC address shown in the DeskBot window (for example 192.168.0.111).";
      form(); return;
    }
    Preferences p;
    p.begin(NS, false);
    p.putString("wifi_ssid", ssid);
    p.putString("wifi_pass", server.arg("pass"));
    String srv = normaliseServer(server.arg("server"));
    if (srv.length()) p.putString("server", srv);
    p.end();
    String body = FPSTR(PAGE_DONE);
    body.replace("%SSID%", htmlEscape(ssid));
    page(body);
    saved = true;
  });
  // Phones and laptops probe these to detect a sign-in page
  for (const char* probe : {"/generate_204", "/gen_204", "/hotspot-detect.html", "/library/test/success.html",
                            "/connecttest.txt", "/ncsi.txt", "/redirect", "/canonical.html"}) {
    server.on(probe, toPortal);
  }
  server.onNotFound(toPortal);
  server.begin();

  String l2 = String("Join Wi-Fi \"") + SETUP_AP_NAME + "\"  password " + SETUP_AP_PASSWORD;
  String l3 = "then open http://" + ip.toString();
  Display::bootMessage("Wi-Fi setup", l2.c_str(), l3.c_str());
  Buzzer::toneBlocking(1319, 90);
  Buzzer::toneBlocking(1760, 120);

  uint32_t t0 = millis(), savedAt = 0;
  while (true) {
    dns.processNextRequest();
    server.handleClient();
    if (saved && !savedAt) savedAt = millis();
    if (savedAt && millis() - savedAt > 2500) break;              // let the "Saved" page load
    if (!saved && timeoutMs && millis() - t0 > timeoutMs) break;  // retry the saved network
    delay(2);
  }
  server.stop();
  dns.stop();
  Display::bootMessage(saved ? "Saved" : "Retrying", saved ? "Restarting..." : "trying saved Wi-Fi again");
  delay(600);
  ESP.restart();
}
