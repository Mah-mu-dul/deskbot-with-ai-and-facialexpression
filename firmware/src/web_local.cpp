#include "web_local.h"
#include "state.h"
#include "net.h"
#include "camera.h"
#include "audio.h"
#include "focus_timer.h"
#include "wifi_setup.h"
#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <driver/gpio.h>
#include <soc/io_mux_reg.h>
#include <soc/gpio_periph.h>

static WebServer server(80);

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>DeskBot</title><style>
:root{--bg:#0d1117;--card:#161b22;--fg:#e6edf3;--mut:#8b949e;--acc:#2fd3ff}
body{margin:0;background:var(--bg);color:var(--fg);font-family:system-ui,sans-serif;padding:16px}
h1{font-size:20px;margin:0 0 12px}.card{background:var(--card);border-radius:12px;padding:14px;margin-bottom:12px}
button{background:#21262d;color:var(--fg);border:1px solid #30363d;border-radius:8px;padding:8px 12px;margin:3px;cursor:pointer}
button:hover{border-color:var(--acc)}input{background:#0d1117;color:var(--fg);border:1px solid #30363d;border-radius:8px;padding:8px;width:min(320px,100%)}
img{max-width:100%;border-radius:8px}.mut{color:var(--mut);font-size:13px}a{color:var(--acc)}
</style></head><body>
<h1>DeskBot (local)</h1>
<div class="card"><div id="st" class="mut">loading...</div>
<p>Full dashboard: <a id="dash" href="#">brain server</a></p></div>
<div class="card"><img id="cam" alt="camera"><br><button onclick="snap()">Refresh photo</button></div>
<div class="card"><b>Focus timer</b><br>
<button onclick="cmd('focus','start')">Start 25/5</button><button onclick="cmd('focus','toggle')">Pause/Resume</button><button onclick="cmd('focus','stop')">Stop</button></div>
<div class="card"><b>Face</b><br><div id="faces"></div></div>
<div class="card"><b>Buzzer</b><br>
<button onclick="cmd('beep','beep')">Beep</button><button onclick="cmd('beep','success')">Success</button><button onclick="cmd('beep','warning')">Warning</button></div>
<div class="card"><b>Brain server URL</b><br><input id="url"><button onclick="saveUrl()">Save</button></div>
<div class="card"><b>Wi-Fi</b><br><span class="mut">Restart into setup mode to choose another network.</span><br><button onclick="wifiSetup()">Change Wi-Fi</button></div>
<script>
const F=['neutral','happy','sad','angry','surprised','sleepy','love','focused','confused','warning'];
document.getElementById('faces').innerHTML=F.map(f=>`<button onclick="cmd('face','${f}',5000)">${f}</button>`).join('');
function cmd(t,v,a){fetch('/api/cmd',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({t,v,a:a||0})})}
function wifiSetup(){if(!confirm('Restart DeskBot into Wi-Fi setup? Then join the "DeskBot-Setup" network (password 11221122).'))return;
fetch('/api/wifi-setup',{method:'POST'}).then(()=>{document.body.innerHTML='<p style="padding:20px">Restarting. Join the Wi-Fi "DeskBot-Setup" (password 11221122) and open http://192.168.4.1</p>'})}
function snap(){document.getElementById('cam').src='/capture?'+Date.now()}
function saveUrl(){fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({server:document.getElementById('url').value})}).then(load)}
async function load(){const s=await (await fetch('/api/status')).json();
document.getElementById('st').innerHTML=`Temp <b>${s.temp??'--'} °C</b> · Humidity <b>${s.hum??'--'} %</b> · Face <b>${s.face}</b><br>Focus <b>${s.focus}</b> ${s.focus!='off'?Math.floor(s.left/60)+':'+String(s.left%60).padStart(2,'0'):''} · Sessions ${s.sessions}<br>Server ${s.serverOk?'online':'offline'} · Camera ${s.camera?'ok':'off'} · IP ${s.ip}`;
const u=document.getElementById('url');if(document.activeElement!==u)u.value=s.server;document.getElementById('dash').href=s.server;}
load();snap();setInterval(load,2000);
</script></body></html>)HTML";

static void sendJson(JsonDocument& doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(code, "application/json", out);
}

static void handleStatus() {
  JsonDocument d;
  if (!isnan(g_state.temperature)) d["temp"] = roundf(g_state.temperature * 10) / 10;
  if (!isnan(g_state.humidity))    d["hum"]  = roundf(g_state.humidity);
  d["face"] = faceName(currentFace());
  d["focus"] = focusPhaseName(g_state.focusPhase);
  d["left"] = (uint32_t)g_state.focusRemainingSec;
  d["total"] = (uint32_t)g_state.focusTotalSec;
  d["sessions"] = (uint16_t)g_state.focusSessions;
  d["volume"] = (uint8_t)g_state.volume;
  d["serverOk"] = (bool)g_state.serverOk;
  d["camera"] = (bool)g_state.cameraOk;
  d["server"] = Net::serverUrl();
  d["ip"] = WiFi.localIP().toString();
  d["uptime"] = millis() / 1000;
  d["heap"] = ESP.getFreeHeap();
  d["psram"] = ESP.getFreePsram();
  sendJson(d);
}

static void handleCapture() {
  camera_fb_t* fb = g_state.cameraOk ? Camera::grab() : nullptr;
  if (!fb) { server.send(503, "text/plain", "camera unavailable"); return; }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.sendContent((const char*)fb->buf, fb->len);
  Camera::release(fb);
}

static void handleCmd() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
  BotCmd c = {};
  strlcpy(c.type, d["t"] | "", sizeof(c.type));
  strlcpy(c.v, d["v"] | "", sizeof(c.v));
  strlcpy(c.text, d["text"] | "", sizeof(c.text));
  c.a = d["a"] | 0;
  c.b = d["b"] | 0;
  bool ok = Net::pushCmd(c);
  server.send(ok ? 200 : 503, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handleConfig() {
  JsonDocument d;
  if (deserializeJson(d, server.arg("plain"))) { server.send(400, "text/plain", "bad json"); return; }
  if (d["server"].is<const char*>()) Net::setServerUrl(d["server"].as<const char*>());
  server.send(200, "application/json", "{\"ok\":true}");
}

void WebLocal::begin() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", PAGE); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/api/cmd", HTTP_POST, handleCmd);
  server.on("/api/config", HTTP_POST, handleConfig);
  server.on("/api/wifi-setup", HTTP_POST, [] {
    WifiSetup::requestPortalOnNextBoot();
    server.send(200, "application/json", "{\"ok\":true}");
    delay(500);
    ESP.restart();
  });
  // Wiring check: sample candidate pins while the I2S clocks run.
  // A pin with mic data shows a mix of highs and lows; idle lines sit at 0 or 100.
  server.on("/api/pinscan", HTTP_GET, [] {
    const int pins[] = {3, 5, 14, 21, 46, 0, 42, 19, 20, 4};
    String out = "{";
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
      int p = pins[i];
      PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[p]);
      uint32_t high = 0, changes = 0;
      int last = gpio_get_level((gpio_num_t)p);
      for (uint32_t k = 0; k < 40000; k++) {
        int v = gpio_get_level((gpio_num_t)p);
        high += v;
        if (v != last) { changes++; last = v; }
      }
      out += "\"" + String(p) + "\":{\"highPct\":" + String(high * 100 / 40000) +
             ",\"changes\":" + String(changes) + "}";
      if (i + 1 < sizeof(pins) / sizeof(pins[0])) out += ",";
    }
    out += "}";
    server.send(200, "application/json", out);
  });

  server.on("/api/mic", HTTP_GET, [] {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    bool rx = server.hasArg("rxonly"), nocam = server.hasArg("nocam");
    server.send(200, "application/json",
                (rx || nocam) ? Audio::micTest(rx, nocam) : Audio::micDebugJson());
  });
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
}

void WebLocal::handle() { server.handleClient(); }
