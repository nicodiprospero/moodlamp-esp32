// MoodLamp — ESP32-S3-N16R8, single WS2812B on pin 48
// Board: esp32 by Espressif Systems 2.0.17
// Library: Adafruit NeoPixel

#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>
#include <math.h>

// ─── Hardware ────────────────────────────────────────────────
#define LED_PIN   48
#define LED_COUNT  1
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ─── Network ─────────────────────────────────────────────────
const char* AP_SSID = "MoodLamp";
const char* AP_PASS = "12345678";
WebServer server(80);

// ─── Modes ───────────────────────────────────────────────────
#define MODE_OFF     0
#define MODE_SOLID   1
#define MODE_BREATHE 2
#define MODE_RAINBOW 3
#define MODE_FIRE    4
#define MODE_PARTY   5

// ─── State ───────────────────────────────────────────────────
int     currentMode = MODE_SOLID;
uint8_t brightness  = 80;   // 0-100
uint8_t speedVal    = 50;   // 0-100

// User-selected base color
uint8_t baseR = 255, baseG = 80, baseB = 0;

// Solid mode: float current + fade target
float curR = 255.0f, curG = 80.0f, curB = 0.0f;
float tgtR = 255.0f, tgtG = 80.0f, tgtB = 0.0f;

// ─── Animation state ─────────────────────────────────────────
unsigned long lastLed       = 0;
unsigned long lastFireTgt   = 0;

float breatheAngle = 0.0f;
float rainbowHue   = 0.0f;
float partyHue     = 0.0f;

float fireR = 220.0f, fireG = 60.0f, fireB = 5.0f;
int   fireTR = 220,   fireTG = 60,   fireTB = 5;

// ─── Utilities ───────────────────────────────────────────────
float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

void hsvToRgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  float c  = v * s;
  float x  = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
  float m  = v - c;
  float rp, gp, bp;
  if      (h < 60.0f)  { rp = c; gp = x; bp = 0; }
  else if (h < 120.0f) { rp = x; gp = c; bp = 0; }
  else if (h < 180.0f) { rp = 0; gp = c; bp = x; }
  else if (h < 240.0f) { rp = 0; gp = x; bp = c; }
  else if (h < 300.0f) { rp = x; gp = 0; bp = c; }
  else                 { rp = c; gp = 0; bp = x; }
  r = (uint8_t)((rp + m) * 255.0f);
  g = (uint8_t)((gp + m) * 255.0f);
  b = (uint8_t)((bp + m) * 255.0f);
}

void setLed(uint8_t r, uint8_t g, uint8_t b) {
  float bf = brightness / 100.0f;
  strip.setPixelColor(0, strip.Color(
    (uint8_t)(r * bf),
    (uint8_t)(g * bf),
    (uint8_t)(b * bf)
  ));
  strip.show();
}

String modeStr(int m) {
  if (m == MODE_OFF)     return "off";
  if (m == MODE_SOLID)   return "solid";
  if (m == MODE_BREATHE) return "breathe";
  if (m == MODE_RAINBOW) return "rainbow";
  if (m == MODE_FIRE)    return "fire";
  if (m == MODE_PARTY)   return "party";
  return "solid";
}

int strToMode(String s) {
  if (s == "off")     return MODE_OFF;
  if (s == "solid")   return MODE_SOLID;
  if (s == "breathe") return MODE_BREATHE;
  if (s == "rainbow") return MODE_RAINBOW;
  if (s == "fire")    return MODE_FIRE;
  if (s == "party")   return MODE_PARTY;
  return MODE_SOLID;
}

// ─── LED Update (called every loop) ─────────────────────────
void updateLed() {
  unsigned long now = millis();
  if (now - lastLed < 20) return;  // ~50 FPS tick
  unsigned long dt = now - lastLed;
  lastLed = now;

  switch (currentMode) {

    case MODE_OFF:
      setLed(0, 0, 0);
      break;

    case MODE_SOLID: {
      // Smooth fade: lerp ~8% per 20ms frame → ~250ms fade
      float t = 0.08f;
      curR = lerpf(curR, tgtR, t);
      curG = lerpf(curG, tgtG, t);
      curB = lerpf(curB, tgtB, t);
      if (fabsf(curR - tgtR) < 0.8f) curR = tgtR;
      if (fabsf(curG - tgtG) < 0.8f) curG = tgtG;
      if (fabsf(curB - tgtB) < 0.8f) curB = tgtB;
      setLed((uint8_t)curR, (uint8_t)curG, (uint8_t)curB);
      break;
    }

    case MODE_BREATHE: {
      // Period: speed=0 → 8s, speed=100 → 0.5s
      float period = 8.0f - (speedVal / 100.0f) * 7.5f;
      float inc = (2.0f * (float)M_PI * (dt / 1000.0f)) / period;
      breatheAngle += inc;
      if (breatheAngle > 2.0f * (float)M_PI) breatheAngle -= 2.0f * (float)M_PI;
      float brt = (sinf(breatheAngle) + 1.0f) / 2.0f;
      brt = brt * brt;  // gamma — softer at low end
      setLed(
        (uint8_t)(baseR * brt),
        (uint8_t)(baseG * brt),
        (uint8_t)(baseB * brt)
      );
      break;
    }

    case MODE_RAINBOW: {
      // Full hue cycle: speed=0 → 20s, speed=100 → 1s
      float cycleTime = 20.0f - (speedVal / 100.0f) * 19.0f;
      rainbowHue += (360.0f * (dt / 1000.0f)) / cycleTime;
      if (rainbowHue >= 360.0f) rainbowHue -= 360.0f;
      uint8_t r, g, b;
      hsvToRgb(rainbowHue, 1.0f, 1.0f, r, g, b);
      setLed(r, g, b);
      break;
    }

    case MODE_FIRE: {
      // New flicker target interval: speed=0 → 300ms, speed=100 → 40ms
      unsigned long interval = (unsigned long)(300.0f - speedVal * 2.6f);
      if (interval < 40) interval = 40;
      if (now - lastFireTgt > interval) {
        lastFireTgt = now;
        fireTR = random(170, 256);   // red: always high
        fireTG = random(20, 110);    // green: low-medium
        fireTB = random(0, 12);      // blue: almost none
        // Rare bright yellow flash
        if (random(10) == 0) {
          fireTR = 255;
          fireTG = random(120, 190);
          fireTB = random(5, 25);
        }
      }
      float ft = 0.18f;
      fireR = lerpf(fireR, (float)fireTR, ft);
      fireG = lerpf(fireG, (float)fireTG, ft);
      fireB = lerpf(fireB, (float)fireTB, ft);
      setLed((uint8_t)fireR, (uint8_t)fireG, (uint8_t)fireB);
      break;
    }

    case MODE_PARTY: {
      // Hue cycle: speed=0 → 5s, speed=100 → 0.3s
      float partyTime = 5.0f - (speedVal / 100.0f) * 4.7f;
      partyHue += (360.0f * (dt / 1000.0f)) / partyTime;
      if (partyHue >= 360.0f) partyHue -= 360.0f;
      uint8_t r, g, b;
      hsvToRgb(partyHue, 1.0f, 1.0f, r, g, b);
      setLed(r, g, b);
      break;
    }
  }
}

// ─── HTML Builder ────────────────────────────────────────────
String buildHtml() {
  String h = "";
  h += "<!DOCTYPE html>";
  h += "<html lang='en'>";
  h += "<head>";
  h += "<meta charset='UTF-8'>";
  h += "<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1'>";
  h += "<title>MoodLamp</title>";
  h += "<style>";
  h += "*{box-sizing:border-box;margin:0;padding:0}";
  h += "body{background:#0d0d1a;color:#e0e0ff;font-family:'Segoe UI',Helvetica,sans-serif;min-height:100vh;padding:16px 14px 32px}";
  h += "h1{text-align:center;font-size:1.35rem;letter-spacing:.12em;color:#9090e0;margin-bottom:18px;font-weight:400}";
  h += "#preview{width:96px;height:96px;border-radius:50%;margin:0 auto 22px;border:3px solid #2a2a4a;transition:background .25s,box-shadow .25s}";
  h += ".modes{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin-bottom:18px}";
  h += ".mb{padding:11px 16px;border-radius:22px;border:2px solid #333360;background:#15152a;color:#9090d0;font-size:.88rem;cursor:pointer;flex:1 1 70px;text-align:center;min-height:44px;transition:all .18s;font-family:inherit}";
  h += ".mb.active{background:#4848b0;border-color:#7070e0;color:#fff;box-shadow:0 0 14px rgba(112,112,224,.45)}";
  h += ".mb:active{transform:scale(.95)}";
  h += ".card{background:#141428;border-radius:14px;padding:14px;margin-bottom:14px;border:1px solid #1e1e3a}";
  h += ".card-title{font-size:.7rem;letter-spacing:.12em;color:#5050a0;text-transform:uppercase;margin-bottom:12px}";
  h += ".colors{display:flex;flex-wrap:wrap;gap:9px}";
  h += ".cd{width:40px;height:40px;border-radius:50%;cursor:pointer;border:3px solid transparent;transition:transform .15s,border-color .15s;flex-shrink:0}";
  h += ".cd.sel{border-color:#fff;transform:scale(1.18)}";
  h += ".cd:active{transform:scale(.9)}";
  h += ".pick-row{display:flex;align-items:center;gap:10px;margin-top:12px}";
  h += ".pick-row span{font-size:.8rem;color:#7070b0}";
  h += "input[type=color]{width:52px;height:40px;border:none;border-radius:8px;cursor:pointer;background:#1a1a3a;padding:2px}";
  h += ".sl-row{display:flex;align-items:center;gap:10px;margin-bottom:12px}";
  h += ".sl-row:last-child{margin-bottom:0}";
  h += ".sl-row label{font-size:.82rem;color:#9090c0;width:78px;flex-shrink:0}";
  h += ".sl-row output{font-size:.82rem;color:#e0e0ff;width:32px;text-align:right;flex-shrink:0}";
  h += "input[type=range]{flex:1;-webkit-appearance:none;height:6px;border-radius:3px;background:#252545;outline:none}";
  h += "input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:24px;height:24px;border-radius:50%;background:#5858cc;cursor:pointer;box-shadow:0 0 8px rgba(88,88,204,.55)}";
  h += "</style>";
  h += "</head>";
  h += "<body>";
  h += "<h1>&#x1F4A1; MoodLamp</h1>";
  h += "<div id='preview'></div>";

  // Mode buttons
  h += "<div class='modes'>";
  h += "<button class='mb' data-mode='off'     onclick='setMode(this)'>Off</button>";
  h += "<button class='mb' data-mode='solid'   onclick='setMode(this)'>Solid</button>";
  h += "<button class='mb' data-mode='breathe' onclick='setMode(this)'>Breathe</button>";
  h += "<button class='mb' data-mode='rainbow' onclick='setMode(this)'>Rainbow</button>";
  h += "<button class='mb' data-mode='fire'    onclick='setMode(this)'>Fire</button>";
  h += "<button class='mb' data-mode='party'   onclick='setMode(this)'>Party</button>";
  h += "</div>";

  // Color card
  h += "<div class='card'>";
  h += "<div class='card-title'>Color</div>";
  h += "<div class='colors' id='cg'></div>";
  h += "<div class='pick-row'>";
  h += "<span>Custom&nbsp;</span>";
  h += "<input type='color' id='cp' oninput='pickColor(this.value)'>";
  h += "</div>";
  h += "</div>";

  // Sliders card
  h += "<div class='card'>";
  h += "<div class='sl-row'>";
  h += "<label>Brightness</label>";
  h += "<input type='range' min='0' max='100' value='80' id='bs' oninput='setBright(this)'>";
  h += "<output id='bv'>80</output>";
  h += "</div>";
  h += "<div class='sl-row'>";
  h += "<label>Speed</label>";
  h += "<input type='range' min='0' max='100' value='50' id='ss' oninput='setSpeed(this)'>";
  h += "<output id='sv'>50</output>";
  h += "</div>";
  h += "</div>";

  // JavaScript
  h += "<script>";

  // Color presets array: [hex, label]
  h += "const P=[";
  h += "['#ff0000','Red'],";
  h += "['#ff3300','Red-Orange'],";
  h += "['#ff6600','Orange'],";
  h += "['#ff9900','Amber'],";
  h += "['#ffcc00','Gold'],";
  h += "['#ffff00','Yellow'],";
  h += "['#99ff00','Lime'],";
  h += "['#00ff00','Green'],";
  h += "['#00ff99','Mint'],";
  h += "['#00ffcc','Teal'],";
  h += "['#00ffff','Cyan'],";
  h += "['#0099ff','Sky'],";
  h += "['#0044ff','Blue'],";
  h += "['#2200ff','Indigo'],";
  h += "['#6600ff','Violet'],";
  h += "['#aa00ff','Purple'],";
  h += "['#ff00ff','Magenta'],";
  h += "['#ff0066','Hot Pink'],";
  h += "['#ff99bb','Blush'],";
  h += "['#ffffff','White'],";
  h += "['#ffe8cc','Warm White'],";
  h += "['#ffddaa','Candle'],";
  h += "['#aaddff','Ice Blue'],";
  h += "['#ffaaff','Lavender']";
  h += "];";

  // Build color grid
  h += "const cg=document.getElementById('cg');";
  h += "P.forEach(([hex,name],i)=>{";
  h += "  const d=document.createElement('div');";
  h += "  d.className='cd';";
  h += "  d.style.background=hex;";
  h += "  d.title=name;";
  h += "  d.onclick=()=>{";
  h += "    document.querySelectorAll('.cd').forEach(x=>x.classList.remove('sel'));";
  h += "    d.classList.add('sel');";
  h += "    sendHex(hex);";
  h += "  };";
  h += "  cg.appendChild(d);";
  h += "});";

  // Helpers
  h += "function hexToRgb(h){";
  h += "  return{r:parseInt(h.slice(1,3),16),g:parseInt(h.slice(3,5),16),b:parseInt(h.slice(5,7),16)};";
  h += "}";

  h += "function sendHex(hex){";
  h += "  const{r,g,b}=hexToRgb(hex);";
  h += "  fetch('/color?r='+r+'&g='+g+'&b='+b);";
  h += "}";

  h += "function pickColor(hex){";
  h += "  document.querySelectorAll('.cd').forEach(x=>x.classList.remove('sel'));";
  h += "  sendHex(hex);";
  h += "}";

  h += "function setMode(btn){";
  h += "  document.querySelectorAll('.mb').forEach(x=>x.classList.remove('active'));";
  h += "  btn.classList.add('active');";
  h += "  fetch('/mode?v='+btn.dataset.mode);";
  h += "}";

  h += "function setBright(el){";
  h += "  document.getElementById('bv').value=el.value;";
  h += "  fetch('/bright?v='+el.value);";
  h += "}";

  h += "function setSpeed(el){";
  h += "  document.getElementById('sv').value=el.value;";
  h += "  fetch('/speed?v='+el.value);";
  h += "}";

  // Live preview polling
  h += "function pollState(){";
  h += "  fetch('/state').then(r=>r.json()).then(s=>{";
  h += "    const c='rgb('+s.r+','+s.g+','+s.b+')';";
  h += "    const glowA=s.mode==='off'?'0':'0.5';";
  h += "    const prev=document.getElementById('preview');";
  h += "    prev.style.background=c;";
  h += "    prev.style.boxShadow='0 0 36px 10px rgba('+s.r+','+s.g+','+s.b+','+glowA+')';";
  h += "    document.getElementById('bs').value=s.bright;";
  h += "    document.getElementById('bv').value=s.bright;";
  h += "    document.getElementById('ss').value=s.speed;";
  h += "    document.getElementById('sv').value=s.speed;";
  h += "    document.querySelectorAll('.mb').forEach(b=>{";
  h += "      b.classList.toggle('active',b.dataset.mode===s.mode);";
  h += "    });";
  h += "  }).catch(()=>{});";
  h += "}";
  h += "setInterval(pollState,500);";
  h += "pollState();";

  h += "</script>";
  h += "</body>";
  h += "</html>";
  return h;
}

// ─── Endpoint Handlers ───────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", buildHtml());
}

void handleColor() {
  if (server.hasArg("r") && server.hasArg("g") && server.hasArg("b")) {
    baseR = (uint8_t)constrain(server.arg("r").toInt(), 0, 255);
    baseG = (uint8_t)constrain(server.arg("g").toInt(), 0, 255);
    baseB = (uint8_t)constrain(server.arg("b").toInt(), 0, 255);
    tgtR = (float)baseR;
    tgtG = (float)baseG;
    tgtB = (float)baseB;
    Serial.print("[color] R="); Serial.print(baseR);
    Serial.print(" G=");        Serial.print(baseG);
    Serial.print(" B=");        Serial.println(baseB);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing r,g,b");
  }
}

void handleBright() {
  if (server.hasArg("v")) {
    brightness = (uint8_t)constrain(server.arg("v").toInt(), 0, 100);
    Serial.print("[bright] "); Serial.println(brightness);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing v");
  }
}

void handleMode() {
  if (server.hasArg("v")) {
    String m = server.arg("v");
    currentMode   = strToMode(m);
    breatheAngle  = 0.0f;
    rainbowHue    = 0.0f;
    partyHue      = 0.0f;
    Serial.print("[mode] "); Serial.println(m);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing v");
  }
}

void handleSpeed() {
  if (server.hasArg("v")) {
    speedVal = (uint8_t)constrain(server.arg("v").toInt(), 0, 100);
    Serial.print("[speed] "); Serial.println(speedVal);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Missing v");
  }
}

void handleState() {
  // Compute what's currently displayed for the preview
  uint8_t dr, dg, db;

  switch (currentMode) {
    case MODE_OFF:
      dr = dg = db = 0;
      break;
    case MODE_SOLID:
      dr = (uint8_t)curR;
      dg = (uint8_t)curG;
      db = (uint8_t)curB;
      break;
    case MODE_BREATHE: {
      float brt = (sinf(breatheAngle) + 1.0f) / 2.0f;
      brt = brt * brt;
      dr = (uint8_t)(baseR * brt);
      dg = (uint8_t)(baseG * brt);
      db = (uint8_t)(baseB * brt);
      break;
    }
    case MODE_RAINBOW: {
      hsvToRgb(rainbowHue, 1.0f, 1.0f, dr, dg, db);
      break;
    }
    case MODE_FIRE:
      dr = (uint8_t)fireR;
      dg = (uint8_t)fireG;
      db = (uint8_t)fireB;
      break;
    case MODE_PARTY: {
      hsvToRgb(partyHue, 1.0f, 1.0f, dr, dg, db);
      break;
    }
    default:
      dr = dg = db = 0;
      break;
  }

  // Apply brightness so preview matches physical LED
  float bf = brightness / 100.0f;
  int pr = (int)(dr * bf);
  int pg = (int)(dg * bf);
  int pb = (int)(db * bf);

  String j = "";
  j += "{\"r\":";
  j += pr;
  j += ",\"g\":";
  j += pg;
  j += ",\"b\":";
  j += pb;
  j += ",\"mode\":\"";
  j += modeStr(currentMode);
  j += "\",\"bright\":";
  j += brightness;
  j += ",\"speed\":";
  j += speedVal;
  j += "}";

  server.sendHeader("Cache-Control", "no-cache");
  server.send(200, "application/json", j);
}

// ─── Setup ───────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("\n=== MoodLamp v2 ===");

  strip.begin();
  strip.setBrightness(255);  // brightness managed manually
  strip.clear();
  strip.show();
  Serial.println("[led] NeoPixel ready on pin 48");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("[wifi] AP '");
  Serial.print(AP_SSID);
  Serial.print("' started. IP: ");
  Serial.println(WiFi.softAPIP());

  server.on("/",       handleRoot);
  server.on("/color",  handleColor);
  server.on("/bright", handleBright);
  server.on("/mode",   handleMode);
  server.on("/speed",  handleSpeed);
  server.on("/state",  handleState);
  server.begin();
  Serial.println("[http] Server started on port 80");

  lastLed     = millis();
  lastFireTgt = millis();
}

// ─── Loop ────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  updateLed();
}
