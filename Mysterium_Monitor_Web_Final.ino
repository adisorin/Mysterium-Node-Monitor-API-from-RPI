/*MIT License

Copyright (c) 2026 adisorin

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <XPT2046_Touchscreen.h>
#include <EEPROM.h>
#include <time.h> // ADAUGAT PENTRU CEAS
#include <WebServer.h> // ADAUGAT PENTRU PAGINA HTML
#include "mysterium_logo.h"
// --- STREAMING_CHUNK:Configuring network and server credentials... ---
// --- Configurare Rețea Wi-Fi (Păstrat exact cum a fost cerut) ---
const char* ssid = "SSID ";
const char* password = "PASSWORD";

// --- Configurare Server Mysterium (TequilAPI pe Raspberry Pi) ---
const char* mystServerIP = "The Raspberry Pi's IP address";
const int mystPort = 4050;
const char* apiUser = "myst";
const char* apiPass = "mystberry";

// --- Variabile globale pentru System Info ---
String g_currentIdentity = "N/A - apasa Refresh";
String g_apiDisplay = String(apiUser) + ":" + String(apiPass);
// --- VARIABILE NOI PENTRU CARDUL 3 ---
String g_nodeVersion = "N/A";
bool g_nodeQualityOk = false;
float g_qualityScore = 0.0; // QUALITY SCORE - scorul real 0.0-1.0 din /node/provider/quality
bool g_timeSynced = false;
// --- VARIABILE NOI PENTRU WEB SERVER (nu afecteaza TFT) ---
double g_totalSettled = 0.0;
double g_unsettled = 0.0;
double g_balance = 0.0;

// --- Server Web pe port 80 - ADAUGAT ---
WebServer server(80);

// --- Pini specifici pentru ESP32-2432S028 (CYD) ---
#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS 15
#define TFT_DC 2
#define TFT_RST -1
#define PIN_BACKLIGHT 21
#define PIN_LDR 34 // ADC1_CH6 pentru senzorul de lumină ambiantă
#define ADC_SAMPLES 5 // Număr de eșantioane pentru media LDR

// --- ADAUGAT TOUCH - Pini XPT2046 HSPI separat (configuratia ta) ---
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI(HSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// --- Paletă de Culori personalizată (RGB565) ---
#define COLOR_BG 0x0000 // Negru profund
#define COLOR_CARD_BG 0x1082 // Gri-albăstrui închis pentru carduri
#define COLOR_TITLE 0x07FF // Cyan vibrant
#define COLOR_TEXT 0xFFFF // Alb pur
#define COLOR_LABEL 0xCE79 // Gri deschis pentru etichete
#define COLOR_STATUS_OK 0x07E0 // Verde aprins
#define COLOR_STATUS_ERR 0xF800 // Roșu aprins
#define COLOR_VALUE_MYST 0xFFE0 // Galben auriu luminos
#define COLOR_ACCENT 0xFD20 // Portocaliu accent

unsigned long lastMillis = 0;
unsigned long lastTimeUpdateMillis = 0; // Pentru ceas
const long interval = 15000; // 15 secunde interval de reîmprospătare date API
const long ldrInterval = 50; // 50ms pentru un răspuns fluid al luminozității

// ==== smoothing ====(factor de netezire al nivelului de lumină)-BACKLIGHT
float smoothBrightness = 5.0; // FIX: pornim luminos, nu 128
const float SMOOTH_FACTOR = 0.03;

// ==== auto-calibrare LDR ====-BACKLIGHT
volatile int ldrMin = 4095;
volatile int ldrMax = 0;

// ==== ADAUGAT TOUCH - Calibrare + System Info ====
int TS_MINX = 200, TS_MAXX = 3800;
int TS_MINY = 200, TS_MAXY = 3800;
bool calibrated = false;
#define EEPROM_SIZE 64
#define ADDR_FLAG 0
#define ADDR_MINX 4
#define ADDR_MAXX 8
#define ADDR_MINY 12
#define ADDR_MAXY 16
#define SYSINFO_BTN_X 290
#define SYSINFO_BTN_Y 4
#define SYSINFO_BTN_W 26
#define SYSINFO_BTN_H 26
#define RESET_BTN_X 288 // ADAUGAT RESET
#define RESET_BTN_Y 18 // ADAUGAT RESET
#define RESET_BTN_R 12 // ADAUGAT RESET
struct TouchPoint { uint16_t x; uint16_t y; bool touched; };
unsigned long lastTouchMillis = 0;
int paginaCurenta = 1;

// Declarații funcții
int readLdrAvg();
void updateBacklightFromLDR();
void drawUIStaticElements();
void fetchMysteriumData();
void fetchNodeVersionAndQuality(); // NOU
void updateBottomInfoDisplay(); // NOU
void updateTimeOnly(); // NOU
// ADAUGAT TOUCH
bool readTouchPoint(TouchPoint &p);
void calibrareTouch();
void handleTouch();
void desenConturEcran();
void desenButonSysInfo();
void afiseazaSystemInfo();
void desenButonResetTFT(); // ADAUGAT RESET
void resetESP(); // ADAUGAT RESET
// ADAUGAT WEB
void setupWebServer();

// --- TASK SEPARAT PENTRU BACKLIGHT - NU E INFLUENTAT DE API ---
void backlightTask(void *pvParameters) {
  // prima citire sa initializeze min/max corect sa nu porneasca cu 0
  delay(300);
  int first = readLdrAvg();
  ldrMin = first - 100;
  ldrMax = first + 200;
  if(ldrMin < 0) ldrMin = 0;// Pe timp de noapte
  if(ldrMax > 4095) ldrMax = 4095;
  smoothBrightness = first; // FIX: pleaca cu valoarea reala, nu blocat

  for(;;){
    updateBacklightFromLDR();
    vTaskDelay(ldrInterval / portTICK_PERIOD_MS);
  }
}


// ==== Citire medie LDR ====-BACKLIGHT
int readLdrAvg() {
  long sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(PIN_LDR);
    delay(5);
  }
  return (int)(sum / ADC_SAMPLES);
}

// --- STREAMING_CHUNK:Implementing LDR backlight control logic... ---
void updateBacklightFromLDR() {
  int ldrValue = readLdrAvg();

  // Auto-calibrare dinamică pentru min și max
  if (ldrValue < ldrMin) ldrMin = ldrValue;
  if (ldrValue > ldrMax) ldrMax = ldrValue;

  // Evităm împărțirea la zero în mapare
  if (ldrMax <= ldrMin) {
    ldrMax = ldrMin + 1;
  }

  // Mapăm valoarea LDR (lumină mare = PWM mare, lumină slabă = PWM mic, minim 20) - VALORILE TALE
  int targetPwm = map(ldrValue, ldrMin, ldrMax, 200, 1);

  // Aplicăm netezirea (smoothing)
  smoothBrightness = smoothBrightness + SMOOTH_FACTOR * (targetPwm - smoothBrightness);

  int pwmValue = (int)smoothBrightness;
  if (pwmValue < 20) pwmValue = 2; // FIX: minim 2, sa nu se stinga complet - VALOAREA TA
  if (pwmValue > 200) pwmValue = 100;

  analogWrite(PIN_BACKLIGHT, pwmValue);
}


//===================== HTML PAGINI - DESIGN CLONA TFT DIN POZA =====================
const char HTML_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1">
<title>Mysterium Node Monitor</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#000;display:flex;justify-content:center;align-items:flex-start;min-height:100vh;font-family:'Share Tech Mono',monospace;padding:10px}
.frame{width:360px;background:#000;padding:6px;border:2px solid #222}
.top{height:40px;background:#0f1e2e;display:flex;align-items:center;justify-content:space-between;padding:0 12px;border-bottom:2px solid #ffb300}
.top h1{color:#00d5ff;font-size:17px;letter-spacing:0.5px}
.ibtn{width:28px;height:28px;border:2px solid #00d5ff;border-radius:50%;color:#00d5ff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:16px;text-decoration:none}
.card{background:#0f1e2e;border-radius:12px;padding:8px 14px;margin:12px 4px}
.c1{border:2px solid #ffb300}.c2{border:2px solid #00e5ff}
.label{color:#7a8a9a;font-size:12px;letter-spacing:0.5px}
.val{font-size:34px;font-weight:900;letter-spacing:1px;margin-top:2px;line-height:1.1}
.v-green{color:#00ff66}
.v-yellow{color:#ffeb3b}
.v-white{color:#e6f0ff}
.small{color:#7a8a9a;font-size:12px;margin-top:4px}
.bottom{border:2px solid #ffb300;border-radius:12px;margin:12px 4px;display:grid;grid-template-columns:1fr 1fr 1fr;overflow:hidden;background:#0f1e2e}
.bcol{padding:8px 8px;background:#0f1e2e;border-right:2px solid #ffb300;display:flex;flex-direction:column;justify-content:center}
.bcol:last-child{border:0}
.bcol .label{font-size:11px;margin-bottom:4px}
.bcol .val{font-size:20px}
.dot{width:20px;height:20px;border-radius:50%;background:#00ff66;display:inline-block;vertical-align:middle;border:2px solid #d0ffd0}
.qwrap{display:flex;align-items:center;gap:6px;margin-top:2px}
.qtext{font-size:13px;line-height:1.1}
</style></head><body><div class="frame">
<div class="top"><h1>Mysterium Node Monitor</h1><a class="ibtn" href="/info">i</a></div>
<div class="card c1"><div class="label">TOTAL SETTLED (MYST):</div><div id="settled" class="val v-green">--</div></div>
<div class="card c2"><div class="label">UNSETTLED (MYST):</div><div id="unsettled" class="val v-yellow">--</div><div id="balance" class="small">Bal: -- MYST</div></div>
<div class="bottom">
<div class="bcol"><div class="label">TIME</div><div id="time" class="val v-white">--:--:--</div></div>
<div class="bcol"><div class="label">NODE VER</div><div id="ver" class="val v-yellow" style="font-size:18px">--</div></div>
<div class="bcol"><div class="label">QUALITY</div><div class="qwrap"><span id="dot" class="dot"></span><div><div id="qscore" class="val v-green" style="font-size:16px">--</div><div id="qtext" class="qtext v-green">GREAT</div></div></div></div>
</div>
</div>
<script>
async function upd(){
 try{
  let r=await fetch('/api/data');let j=await r.json();
  document.getElementById('settled').innerText=j.settled.toFixed(8);
  document.getElementById('unsettled').innerText=j.unsettled.toFixed(8);
  document.getElementById('balance').innerText='Bal: '+j.balance.toFixed(8)+' MYST';
  document.getElementById('ver').innerText=j.version;
  document.getElementById('time').innerText=j.time;
  document.getElementById('qscore').innerText=j.qualityScore.toFixed(2);
  document.getElementById('qtext').innerText=j.qualityOk?'GREAT':'POOR';
  let ok=j.qualityOk;
  document.getElementById('dot').style.background=ok?'#00ff66':'#ff3333';
  document.getElementById('qscore').className='val '+(ok?'v-green':'v-white');
  document.getElementById('qtext').className='qtext '+(ok?'v-green':'v-white');
  document.getElementById('qtext').style.color=ok?'#00ff66':'#ff5555';
 }catch(e){}
}
setInterval(upd,1000);upd();
</script></body></html>
)rawliteral";

const char HTML_INFO[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>System Info</title>
<link href="https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap" rel="stylesheet">
<style>
*{box-sizing:border-box}body{background:#000;display:flex;justify-content:center;padding:10px;font-family:'Share Tech Mono',monospace;color:#fff}
.frame{width:360px;background:#000;padding:6px;border:2px solid #222}
.top{height:40px;background:#0f1e2e;display:flex;align-items:center;justify-content:space-between;padding:0 12px;border-bottom:2px solid #ffb300}
.top h1{color:#00d5ff;font-size:17px;letter-spacing:0.5px}
.ibtn{width:28px;height:28px;border:2px solid #00d5ff;border-radius:50%;color:#00d5ff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:24px;line-height:1;padding:0 0 3px 0;text-decoration:none}

.rbtn{width:28px;height:28px;border-radius:50%;background:#ff0000;color:#fff;display:flex;align-items:center;justify-content:center;font-weight:900;font-size:16px;text-decoration:none;border:2px solid #ff4444}
.actions{display:flex;gap:8px;align-items:center}

.card{background:#0f1e2e;border:2px solid #ffb300;border-radius:12px;padding:12px;margin:12px 4px;line-height:22px;font-size:13px}
.label{color:#ffb300;font-size:11px;margin-top:8px}.val{color:#e6f0ff}.cyan{color:#00d5ff}.yellow{color:#ffeb3b}.green{color:#00ff66}
hr{border:0;border-top:1px solid #ffb300;margin:10px 0}
</style></head><body><div class="frame">
<div class="top"><h1>SYSTEM INFO</h1><div class="actions"><a class="rbtn" href="/reset">R</a><a class="ibtn" href="/">&#8592;</a></div></div>
<div id="info" class="card">Loading...</div>
</div>
<script>
async function u(){
 try{
  let r=await fetch('/api/system');let j=await r.json();
  document.getElementById('info').innerHTML=
`CPU: ESP32 @ ${j.cpu} MHz<br>Flash: ${j.flash} MB<br>Heap Free: ${j.heap} KB<br>Uptime: ${String(Math.floor(j.uptime/3600)).padStart(3,'0')}:${String(Math.floor(j.uptime%3600/60)).padStart(2,'0')}:${String(Math.floor(j.uptime%60)).padStart(2,'0')}<br>Temp CPU: ${j.temp.toFixed(1)} C<br>TFT: ILI9341 320x240<br>Touch: XPT2046 HSPI<br>
IP: <span class="cyan">${j.ip}</span><br>RSSI: ${j.rssi} dBm<br>
<hr><div class="label">API SERVER:</div><div class="val">${j.apiServer}</div>
<div class="label">API KEY (User:Pass):</div><div class="val yellow">${j.apiKey}</div>
<div class="label">YOUR IDENTITY:</div><div class="val green" style="word-break:break-all;font-size:11px">${j.identity}</div>`;
 }catch(e){document.getElementById('info').innerText='Eroare load';}
}
setInterval(u,1000);u();
</script></body></html>
)rawliteral";

// --- STREAMING_CHUNK:Initializing system setup... ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n--- INITIALIZARE SISTEM MYSTERIUM MONITOR ---");

  // Configurare pini Backlight și LDR (ADC)
  pinMode(PIN_BACKLIGHT, OUTPUT);
  pinMode(PIN_LDR, INPUT);

  // Inițializare SPI hardware pe pini specifici CYD
  SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);

  // --- ADAUGAT TOUCH - init EEPROM + HSPI ---
  EEPROM.begin(EEPROM_SIZE);
  int flag = 0; EEPROM.get(ADDR_FLAG, flag);
  if (flag == 1234) {
    EEPROM.get(ADDR_MINX, TS_MINX); EEPROM.get(ADDR_MAXX, TS_MAXX);
    EEPROM.get(ADDR_MINY, TS_MINY); EEPROM.get(ADDR_MAXY, TS_MAXY);
    calibrated = true;
  }

  tft.begin();
  tft.setRotation(1); // Landscape (320x240)
  tft.fillScreen(COLOR_BG);

  // --- BOOT LOGO 1:1 DIN POZA TA ---
  tft.drawRGBBitmap((320-MYST_LOGO_W)/2, (240-MYST_LOGO_H)/2 - 15, mysteriumLogoBitmap, MYST_LOGO_W, MYST_LOGO_H);
  analogWrite(PIN_BACKLIGHT, 150);
  delay(5000);
  // --- END BOOT LOGO ---
  // Setăm un nivel mediu inițial pentru backlight - FIX 150 ca sa vezi ecranul
//  analogWrite(PIN_BACKLIGHT, 2);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchscreenSPI);
  ts.setRotation(1);
  if (ts.touched()) { delay(1000); if (ts.touched()) calibrareTouch(); }
  if (!calibrated) calibrareTouch();

  // Ecran de încărcare colorat
  tft.fillRect(40, 90, 240, 60, COLOR_CARD_BG);
  tft.drawRect(40, 90, 240, 60, COLOR_ACCENT);

  tft.setCursor(60, 112);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.print("Conectare Wi-Fi...");

  Serial.printf("Conectare la reteaua Wi-Fi: [%s]\n", ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status()!= WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 40) {
      Serial.println("\nEroare: Conectarea Wi-Fi a eșuat. Verifică datele rețelei.");
      tft.fillRect(40, 90, 240, 60, COLOR_CARD_BG);
      tft.setCursor(55, 112);
      tft.setTextColor(COLOR_STATUS_ERR);
      tft.setTextSize(2);
      tft.print("Wi-Fi EROARE!");
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Conectat cu succes!");
    Serial.print("Adresa IP ESP32: ");
    Serial.println(WiFi.localIP());
    // --- NOU: Sincronizare timp NTP (Romania GMT+3 vara, GMT+2 iarna) ---
    configTime(7200, 3600, "pool.ntp.org", "time.nist.gov");
    // set timezone string pentru Bucuresti
    setenv("TZ", "EET-2EEST,M3.5.0/3,M10.5.0/4", 1);
    tzset();

    // --- ADAUGAT WEB SERVER ---
    setupWebServer();
  }

  drawUIStaticElements();

  // FIX API: prima citire imediata, nu dupa 15 sec
  if (WiFi.status() == WL_CONNECTED) {
    fetchMysteriumData();
    fetchNodeVersionAndQuality(); // NOU
    lastMillis = millis();
  }

  // Pornim task-ul de lumina pe CORE 1 - complet independent de API si WiFi (care sunt pe CORE 0)
  xTaskCreatePinnedToCore(backlightTask, "BacklightTask", 4096, NULL, 2, NULL, 1);
}

// --- STREAMING_CHUNK:Running main application loop... ---
void loop() {
  // --- ADAUGAT WEB - trebuie apelat mereu ---
  server.handleClient();

  // LUMINA NU MAI E AICI - E IN TASK-UL SEPARAT

  // --- ADAUGAT TOUCH - verificare atingere ---
  handleTouch();

  // --- NOU: Update ceas la fiecare 1 sec ---
  if (millis() - lastTimeUpdateMillis >= 1000) {
    lastTimeUpdateMillis = millis();
    if (paginaCurenta == 1) {
      updateTimeOnly();
    }
  }

  // Interogare API Mysterium la intervalul stabilit
  if (millis() - lastMillis >= interval) {
    lastMillis = millis();
    if (WiFi.status() == WL_CONNECTED) {
      if (paginaCurenta == 1) {
        fetchMysteriumData();
        fetchNodeVersionAndQuality();
      }
    } else {
      Serial.println("Wi-Fi deconectat. Reîncerc reconectarea...");
      WiFi.begin(ssid, password);
      // Mesaj de atentionare pe TFT
      tft.fillRect(20, 55, 280, 25, COLOR_CARD_BG);
      tft.setCursor(20, 58);
      tft.setTextColor(COLOR_STATUS_ERR);
      tft.setTextSize(2);
      tft.print("Wi-Fi OFF");
      g_nodeQualityOk = false;
      updateBottomInfoDisplay();
    }
  }
}



// --- STREAMING_CHUNK:Rendering static UI elements... ---
void drawUIStaticElements() {
  tft.fillScreen(COLOR_BG);

  // Bara de titlu sus
  tft.fillRect(0, 0, 320, 35, COLOR_CARD_BG);
  tft.drawFastHLine(0, 35, 320, COLOR_ACCENT);

  tft.setCursor(15, 10);
  tft.setTextColor(COLOR_TITLE);
  tft.setTextSize(2);
  tft.print("Mysterium Node Monitor");

  // === MODIFICAT: Carduri mai mici pe verticala ===

  // Card 1: Total Settled - inaltime 52 in loc de 75
  tft.fillRoundRect(10, 45, 300, 52, 6, COLOR_CARD_BG);
  tft.drawRoundRect(10, 45, 300, 52, 6, COLOR_ACCENT);

  tft.setCursor(20, 49);
  tft.setTextColor(COLOR_LABEL);
  tft.setTextSize(1);
  tft.print("TOTAL SETTLED (MYST):");

  // Card 2: Unsettled Earnings - inaltime 70 in loc de 95, mutat mai sus
  tft.fillRoundRect(10, 105, 300, 70, 6, COLOR_CARD_BG);
  tft.drawRoundRect(10, 105, 300, 70, 6, COLOR_TITLE);

  tft.setCursor(20, 109);
  tft.setTextColor(COLOR_LABEL);
  tft.setTextSize(1);
  tft.print("UNSETTLED (MYST):");

  // === NOU: Card 3 jos cu 3 sectiuni ===
  tft.fillRoundRect(10, 183, 300, 50, 6, COLOR_CARD_BG);
  tft.drawRoundRect(10, 183, 300, 50, 6, COLOR_ACCENT);

  // Linii verticale separator
  tft.drawFastVLine(110, 183, 50, COLOR_ACCENT);
  tft.drawFastVLine(210, 183, 50, COLOR_ACCENT);

  // Etichete card 3
  tft.setTextSize(1);
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(18, 187); tft.print("TIME");
  tft.setCursor(130, 187); tft.print("NODE VER");
  tft.setCursor(225, 187); tft.print("QUALITY");

  // --- ADAUGAT TOUCH - buton info ---
  desenButonSysInfo();
}

// --- NOU: Functii pentru cardul de jos ---
void updateTimeOnly() {
  if (paginaCurenta!= 1) return;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // Daca nu e sincronizat, afisam uptime
    tft.fillRect(12, 200, 96, 28, COLOR_CARD_BG);
    tft.setCursor(18, 205);
    tft.setTextColor(COLOR_TEXT);
    tft.setTextSize(2);
    unsigned long sec = millis() / 1000;
    tft.printf("%02lu:%02lu:%02lu", (sec/3600)%24, (sec/60)%60, sec%60);
    return;
  }
  tft.fillRect(12, 200, 96, 28, COLOR_CARD_BG);
  tft.setCursor(14, 203);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.printf("%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

// Afiseaza versiunea si calitatea jos pe ecran - REAL NELIMITAT
void updateBottomInfoDisplay() {
  if (paginaCurenta!= 1) return;
  // Versiune nod - stanga jos
  tft.fillRect(112, 200, 96, 28, COLOR_CARD_BG);
  tft.setCursor(125, 203);
  tft.setTextColor(COLOR_VALUE_MYST);
  tft.setTextSize(2);
  tft.print(g_nodeVersion);

  // Calitate - dreapta jos - REAL NELIMITAT
  tft.fillRect(212, 198, 96, 32, COLOR_CARD_BG);
  bool isGreat = (g_qualityScore > 0.7f); // prag GREAT de la 0.7 in sus
  int dotColor = isGreat? COLOR_STATUS_OK : COLOR_STATUS_ERR; // verde daca e bun, rosu daca e prost
  tft.fillCircle(230, 213, 10, dotColor); // bulina colorata
  tft.drawCircle(230, 213, 10, COLOR_TEXT);
  tft.setCursor(250, 203);
  tft.setTextColor(dotColor);
  tft.setTextSize(1);
  tft.printf("%.2f", g_qualityScore); // arata real: 0.34, 2.07, 5.00 etc - FARA LIMITA
  tft.setCursor(250, 213);
  tft.print(isGreat? "GREAT" : "POOR");
  updateTimeOnly();
}

// Ia versiunea si calitatea REALA din nodul Mysterium - FARA EXEMPLE, FARA CLAMP
void fetchNodeVersionAndQuality() {
  HTTPClient http;
  String baseUrl = String("http://") + mystServerIP + ":" + mystPort;
  String url;
  int code;

  // 1. Versiune din /healthcheck
  url = baseUrl + "/healthcheck";
  http.begin(url);
  http.setAuthorization(apiUser, apiPass); // auth cu user/pass
  http.setTimeout(3000);
  code = http.GET();
  if (code == 200) {
    String payload = http.getString();
    Serial.println("[healthcheck] " + payload); // RAW real din nod
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      String ver = "N/A";
      if (doc["version"].is<const char*>()) ver = doc["version"].as<String>();
      else if (doc["buildInfo"]["version"].is<const char*>()) ver = doc["buildInfo"]["version"].as<String>();
      int dash = ver.indexOf('-');
      if (dash > 0) ver = ver.substring(0, dash); // taie -berry etc
      if (ver.length() > 12) ver = ver.substring(0, 12);
      g_nodeVersion = ver; // salveaza versiunea reala
    }
  }
  http.end();

  // 2. Calitate REALA din /node/provider/quality - NELIMITATA
  url = baseUrl + "/node/provider/quality";
  http.begin(url);
  http.setAuthorization(apiUser, apiPass);
  http.setTimeout(3000);
  code = http.GET();

  if (code == 200) {
    String payload = http.getString();
    Serial.println("[quality RAW] " + payload); // ex real: {"quality":2.07}
    JsonDocument doc;
    if (!deserializeJson(doc, payload)) {
      float raw = -1;
      // incearca toate variantele de camp
      if (doc["quality"].is<float>() || doc["quality"].is<double>()) raw = doc["quality"].as<float>();
      else if (doc["quality"].is<int>() || doc["quality"].is<long>()) raw = doc["quality"].as<float>();
      else if (doc["quality"].is<const char*>()) raw = String(doc["quality"].as<const char*>()).toFloat();
      else if (doc["quality"]["quality"].is<float>()) raw = doc["quality"]["quality"].as<float>();
      else if (doc["proposalQuality"].is<float>()) raw = doc["proposalQuality"].as<float>();

      Serial.printf("[quality parsed] raw=%.2f\n", raw); // raw real nelimitat

      if (raw >= 0) {
        g_qualityScore = raw; // REAL NELIMITAT - poate fi 0.34, 2.07, 10.00 etc
        g_nodeQualityOk = (g_qualityScore > 0.7f); // GREAT de la 0.7 in sus
        Serial.printf("[Quality] OK -> %.2f ok=%d\n", g_qualityScore, g_nodeQualityOk);
      }
    }
  } else {
    // Daca API-ul da 500, e unknown real - nu inventam valoare
    Serial.printf("[quality] HTTP %d - real e unknown momentan\n", code);
    g_qualityScore = 0.0f;
    g_nodeQualityOk = false;
  }
  http.end();
  updateBottomInfoDisplay(); // actualizeaza TFT cu valoarea reala
}

// --- STREAMING_CHUNK:Fetching and parsing earnings data from API... ---
// --- FUNCTIE CORECTATA PENTRU VALOARE REALA DE PE NODE (COMBINATA CU MESAJE TFT) ---
void fetchMysteriumData() {
  HTTPClient http;
  String baseUrl = String("http://") + mystServerIP + ":" + mystPort;

  // PASUL 1: Luam lista de identitati ca sa aflam ID-ul
  String urlIdentities = baseUrl + "/identities";
  Serial.println("\n----------------------------------------");
  Serial.println("[1] Interoghez: " + urlIdentities);

  http.begin(urlIdentities);
  http.setAuthorization(apiUser, apiPass);
  http.setTimeout(5000);
  int httpCode = http.GET();

  Serial.print("Cod HTTP primit: ");
  Serial.println(httpCode);

  if (httpCode!= 200) {
    Serial.printf("Eroare la /identities: %d\n", httpCode);
    // --- Mesaj de atentionare pe TFT pastrat dar adaptat pentru carduri mici ---
    tft.fillRect(20, 65, 280, 25, COLOR_CARD_BG);
    tft.setCursor(20, 68);
    tft.setTextColor(COLOR_STATUS_ERR);
    tft.setTextSize(2);
    tft.print("EROARE API");
    g_nodeQualityOk = false;
    updateBottomInfoDisplay();
    http.end();
    return;
  }

  String payload = http.getString();
  Serial.println("--- RASPUNS API BRUT (JSON) ---");
  Serial.println(payload);
  Serial.println("----------------------------------------");

  // In ArduinoJson 7 folosim JsonDocument
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);
  http.end();

  if (error) {
    Serial.print("JSON error identities: ");
    Serial.println(error.c_str());
    tft.fillRect(20, 65, 280, 25, COLOR_CARD_BG);
    tft.setCursor(20, 68);
    tft.setTextColor(COLOR_STATUS_ERR);
    tft.setTextSize(2);
    tft.print("JSON ERROR");
    g_nodeQualityOk = false;
    updateBottomInfoDisplay();
    return;
  }

  JsonArray identities = doc["identities"];
  if (identities.isNull() || identities.size() == 0) {
    Serial.println("Nu exista nicio identitate pe nod!");
    tft.fillRect(20, 65, 280, 25, COLOR_CARD_BG);
    tft.setCursor(20, 68);
    tft.setTextColor(COLOR_STATUS_ERR);
    tft.setTextSize(2);
    tft.print("ID LIPSA");
    g_nodeQualityOk = false;
    updateBottomInfoDisplay();
    return;
  }

  String identityId = identities[0]["id"].as<String>();
  Serial.println("Identitate gasita: " + identityId);

  // --- SALVAM PENTRU PAGINA DE SYSTEM INFO ---
  g_currentIdentity = identityId;

  // PASUL 2: Luam detaliile REALE pentru identitatea asta
  String urlDetail = baseUrl + "/identities/" + identityId;
  Serial.println("[2] Interoghez: " + urlDetail);

  http.begin(urlDetail);
  http.setAuthorization(apiUser, apiPass);
  http.setTimeout(5000);
  httpCode = http.GET();

  Serial.print("Cod HTTP detalii: ");
  Serial.println(httpCode);

  if (httpCode!= 200) {
    Serial.printf("Eroare la /identities/id: %d\n", httpCode);
    tft.fillRect(20, 65, 280, 25, COLOR_CARD_BG);
    tft.setCursor(20, 68);
    tft.setTextColor(COLOR_STATUS_ERR);
    tft.setTextSize(2);
    tft.print("EROARE API");
    g_nodeQualityOk = false;
    updateBottomInfoDisplay();
    http.end();
    return;
  }

  String payloadDetail = http.getString();
  Serial.println("--- RASPUNS DETALII BRUT ---");
  Serial.println(payloadDetail);
  Serial.println("----------------------------------------");

  JsonDocument doc2;
  error = deserializeJson(doc2, payloadDetail);
  http.end();

  if (error) {
    Serial.print("JSON error detail: ");
    Serial.println(error.c_str());
    tft.fillRect(20, 65, 280, 25, COLOR_CARD_BG);
    tft.setCursor(20, 68);
    tft.setTextColor(COLOR_STATUS_ERR);
    tft.setTextSize(2);
    tft.print("JSON ERROR");
    g_nodeQualityOk = false;
    updateBottomInfoDisplay();
    return;
  }

  // Aici vin valorile in WEI - pot fi foarte mari, le luam ca double
  double balanceWei = doc2["balance"].as<double>();
  double earningsWei = doc2["earnings"].as<double>(); // asta e unsettled
  double earningsTotalWei = doc2["earningsTotal"].as<double>(); // asta e total settled

  const double WEI_TO_MYST = 1e18;
  double totalSettled = earningsTotalWei / WEI_TO_MYST;
  double unsettled = earningsWei / WEI_TO_MYST;
  double balance = balanceWei / WEI_TO_MYST;

  if (totalSettled == 0) {
     double fallback = (doc2["earnings_total"] | doc2["earningsTotal"] | 0.0);
     if (fallback > 1000000) fallback /= WEI_TO_MYST;
     totalSettled = fallback;
  }

  // --- SALVAM SI PENTRU WEB ---
  g_totalSettled = totalSettled;
  g_unsettled = unsettled;
  g_balance = balance;

  Serial.printf("VALORI REALE -> Total Settled: %.8f | Unsettled: %.8f MYST\n", totalSettled, unsettled);

  // --- AFISARE PE ECRAN - cu culorile tale originale dar pe carduri mici ---
  // Sterge zona veche si afiseaza Total
  tft.fillRect(20, 62, 280, 30, COLOR_CARD_BG);
  tft.setCursor(20, 65);
  tft.setTextColor(COLOR_STATUS_OK);
  tft.setTextSize(3);
  tft.printf("%.8f", totalSettled);

  // Sterge zona veche si afiseaza Unsettled
  tft.fillRect(20, 125, 280, 30, COLOR_CARD_BG);
  tft.setCursor(20, 128);
  tft.setTextColor(COLOR_VALUE_MYST);
  tft.setTextSize(3);
  tft.printf("%.8f", unsettled);

  // Afisare balance in colt
  tft.fillRect(20, 155, 280, 15, COLOR_CARD_BG);
  tft.setCursor(20, 158);
  tft.setTextColor(COLOR_LABEL);
  tft.setTextSize(1);
  tft.printf("Bal: %.8f MYST", balance);

  g_nodeQualityOk = true;
  updateBottomInfoDisplay();
}

// =================== ADAUGAT TOUCH - FUNCTII NOI ===================
void desenConturEcran() { tft.drawRect(0, 0, 320, 240, COLOR_ACCENT); }

bool readTouchPoint(TouchPoint &p) {
  if (!ts.touched()) return false;
  TS_Point pt = ts.getPoint();
  if (pt.z < 200) return false;
  if (!calibrated) { p.x = pt.x; p.y = pt.y; }
  else {
    p.x = map(pt.x, TS_MINX, TS_MAXX, 0, 320);
    p.y = map(pt.y, TS_MINY, TS_MAXY, 0, 240);
  }
  p.touched = true;
  delay(75);
  return true;
}

void handleTouch() {
  TouchPoint p;
  if (!readTouchPoint(p)) return;
  if (millis() - lastTouchMillis < 400) return;
  lastTouchMillis = millis();
  if (p.x > SYSINFO_BTN_X && p.x < SYSINFO_BTN_X + SYSINFO_BTN_W && p.y > SYSINFO_BTN_Y && p.y < SYSINFO_BTN_Y + SYSINFO_BTN_H) {
    afiseazaSystemInfo(); return;
  }
  if (p.x > 10 && p.x < 310 && p.y > 45 && p.y < 225 && paginaCurenta == 1) {
    lastMillis = 0;
    fetchMysteriumData();
    fetchNodeVersionAndQuality();
  }
  while (ts.touched()) delay(10);
}

void calibrareTouch() {
  analogWrite(PIN_BACKLIGHT, 30);
  int minX = 9999, maxX = 0, minY = 9999, maxY = 0;
  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_STATUS_ERR);
  tft.setTextSize(2);
  int puncte[4][2] = {{10, 10}, {310, 10}, {10, 230}, {310, 230}};
  for (int i = 0; i < 4; i++) {
    tft.fillScreen(COLOR_BG);
    desenConturEcran();
    tft.setCursor(60, 100);
    tft.println("Atinge punctul!");
    tft.fillCircle(puncte[i][0], puncte[i][1], 8, COLOR_STATUS_ERR);
    bool ok = false;
    while (!ok) {
      if (ts.touched()) {
        TS_Point pt = ts.getPoint();
        if (pt.x < minX) minX = pt.x; if (pt.x > maxX) maxX = pt.x;
        if (pt.y < minY) minY = pt.y; if (pt.y > maxY) maxY = pt.y;
        ok = true; delay(500); while (ts.touched()) delay(10);
      }
    }
  }
  TS_MINX = minX; TS_MAXX = maxX; TS_MINY = minY; TS_MAXY = maxY; calibrated = true;
  int flag = 1234;
  EEPROM.put(ADDR_FLAG, flag); EEPROM.put(ADDR_MINX, TS_MINX);
  EEPROM.put(ADDR_MAXX, TS_MAXX); EEPROM.put(ADDR_MINY, TS_MINY); EEPROM.put(ADDR_MAXY, TS_MAXY);
  EEPROM.commit();
  tft.fillScreen(COLOR_BG);
  tft.setCursor(40, 100); tft.setTextColor(COLOR_STATUS_OK); tft.println("Calibrare OK!"); delay(1000);
}

void desenButonSysInfo() {
  tft.fillRoundRect(SYSINFO_BTN_X, SYSINFO_BTN_Y, SYSINFO_BTN_W, SYSINFO_BTN_H, 8, COLOR_CARD_BG);
  tft.drawRoundRect(SYSINFO_BTN_X, SYSINFO_BTN_Y, SYSINFO_BTN_W, SYSINFO_BTN_H, 8, COLOR_TITLE);
  tft.setCursor(SYSINFO_BTN_X + 9, SYSINFO_BTN_Y + 6);
  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE);
  tft.print("i");
}

void desenButonResetTFT() {
  tft.fillCircle(RESET_BTN_X, RESET_BTN_Y, RESET_BTN_R, COLOR_STATUS_ERR);
  tft.drawCircle(RESET_BTN_X, RESET_BTN_Y, RESET_BTN_R, COLOR_TEXT);
  tft.setCursor(RESET_BTN_X - 4, RESET_BTN_Y - 6);
  tft.setTextColor(COLOR_TEXT);
  tft.setTextSize(2);
  tft.print("R");
}
void resetESP() { ESP.restart(); }

void afiseazaSystemInfo() {
  paginaCurenta = 2;
  tft.fillScreen(COLOR_BG);
  desenConturEcran();
  tft.setTextSize(2); tft.setTextColor(COLOR_VALUE_MYST);
  int y = 15; tft.setCursor(10, y); tft.print("SYSTEM INFO"); y += 20;
  tft.drawFastHLine(0, y, 320, COLOR_ACCENT); y += 10;
  tft.setTextSize(1); tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, y); tft.printf("CPU: ESP32 @ %lu MHz", ESP.getCpuFreqMHz()); y += 14;
  tft.setCursor(10, y); tft.printf("Flash: %lu MB", ESP.getFlashChipSize()/1024/1024); y += 14;
  tft.setCursor(10, y); tft.printf("Heap Free: %lu KB", ESP.getFreeHeap()/1024); y += 14;
  tft.setCursor(10, y); tft.printf("TFT: ILI9341 320x240"); y += 14;
  tft.setCursor(10, y); tft.printf("Touch: XPT2046 HSPI"); y += 14;
  tft.setCursor(10, y); tft.printf("IP: %s", WiFi.localIP().toString().c_str()); y += 14;
  tft.setCursor(10, y); tft.printf("RSSI: %d dBm", WiFi.RSSI()); y += 14;
  #if defined(CONFIG_IDF_TARGET_ESP32)
    float temp = temperatureRead();
    tft.setCursor(10, y); tft.printf("Temp CPU: %.1f C", temp); y += 14;
  #endif

  y += 2;
  tft.drawFastHLine(0, y, 320, COLOR_ACCENT); y += 6;
  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y); tft.print("API SERVER:"); y += 12;
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(10, y); tft.printf("%s:%d", mystServerIP, mystPort); y += 14;

  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y); tft.print("API KEY (User:Pass):"); y += 12;
  tft.setTextColor(COLOR_VALUE_MYST);
  tft.setCursor(10, y); tft.printf("%s:%s", apiUser, apiPass); y += 14;

  tft.setTextColor(COLOR_LABEL);
  tft.setCursor(10, y); tft.print("YOUR IDENTITY:"); y += 12;
  tft.setTextColor(COLOR_STATUS_OK);
  tft.setCursor(10, y);
  if (g_currentIdentity.length() > 26) {
    tft.print(g_currentIdentity.substring(0, 45));
  }

  desenButonResetTFT(); // ADAUGAT RESET

  unsigned long start = millis();
  while (millis() - start < 25000) {
    server.handleClient();
    TouchPoint rp;
    if (readTouchPoint(rp)) {
      int dx = rp.x - RESET_BTN_X;
      int dy = rp.y - RESET_BTN_Y;
      if (dx*dx + dy*dy <= (RESET_BTN_R+5)*(RESET_BTN_R+5)) {
        resetESP();
      }
      while (ts.touched()) delay(300);
      break;
    }
    delay(100);
  }
  paginaCurenta = 1;
  drawUIStaticElements();
  fetchMysteriumData();
  fetchNodeVersionAndQuality();
}

// =================== ADAUGAT WEB SERVER - NU MODIFICA TFT ===================
void setupWebServer(){
  server.on("/", [](){ server.send_P(200,"text/html",HTML_MAIN); });
  server.on("/info", [](){ server.send_P(200,"text/html",HTML_INFO); });
server.on("/reset", HTTP_GET, [](){
  // HTML cu chenar albastru vizibil
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'>"
                "<meta name='viewport' content='width=device-width, initial-scale=1'>"
                "<style>"
                "body{background:#0f172a;display:flex;justify-content:center;align-items:center;height:100vh;margin:0;font-family:Arial, sans-serif;}"
                ".box{border:3px solid #0066ff;background:#ffffff;padding:40px 60px;border-radius:12px;box-shadow:0 0 20px rgba(0,102,255,0.5);text-align:center;}"
                ".box h1{color:#0066ff;margin:0;font-size:28px;letter-spacing:2px;}"
                ".box p{color:#333;margin-top:10px;}"
                "</style></head><body>"
                "<div class='box'><h1>ESP RESETAT</h1><p>Se restarteaza...</p></div>"
                "<script>setTimeout(()=>{window.location.href='/';},3000);</script>"
                "</body></html>";
  server.send(200, "text/html", html);
  delay(1500); // timp sa trimita HTML-ul
  ESP.restart(); // reset real
});

  server.on("/api/data", [](){
    struct tm timeinfo; char tbuf[20]="--:--:--";
    if(getLocalTime(&timeinfo)) strftime(tbuf,20,"%H:%M:%S",&timeinfo);
    else { unsigned long s=millis()/1000; snprintf(tbuf,20,"%02lu:%02lu:%02lu",(s/3600)%24,(s/60)%60,s%60); }
    JsonDocument doc;
    doc["settled"]=g_totalSettled;
    doc["unsettled"]=g_unsettled;
    doc["balance"]=g_balance;
    doc["version"]=g_nodeVersion;
    doc["qualityOk"]=g_nodeQualityOk;
    doc["qualityScore"]=g_qualityScore;
    doc["time"]=tbuf;
    String out; serializeJson(doc,out);
    server.send(200,"application/json",out);
  });

  server.on("/api/system", [](){
    float temp = 0;
    #if defined(CONFIG_IDF_TARGET_ESP32)
      temp = temperatureRead();
    #endif
    JsonDocument doc;
    doc["cpu"]=ESP.getCpuFreqMHz();
    doc["flash"]=ESP.getFlashChipSize()/1024/1024;
    doc["heap"]=ESP.getFreeHeap()/1024;
    doc["uptime"]=millis()/1000;
    doc["temp"]=temp;
    doc["ip"]=WiFi.localIP().toString();
    doc["rssi"]=WiFi.RSSI();
    doc["apiServer"]=String(mystServerIP)+":"+String(mystPort);
    doc["apiKey"]=String(apiUser)+":"+String(apiPass);
    doc["identity"]=g_currentIdentity;
    doc["version"]=g_nodeVersion;
    doc["qualityOk"]=g_nodeQualityOk;
    doc["qualityScore"]=g_qualityScore;
    doc["settled"]=g_totalSettled;
    doc["unsettled"]=g_unsettled;
    doc["balance"]=g_balance;
    String out; serializeJson(doc,out);
    server.send(200,"application/json",out);
  });

  server.begin();
  Serial.println("WebServer pornit: http://" + WiFi.localIP().toString() + "/");
}