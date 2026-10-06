#include <Arduino.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <GT7UDPParser.h>
#include <Preferences.h>

// ==================== CONFIGURAZIONE LED ====================
#define LED_PIN     7
#define MAX_LEDS    200
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[MAX_LEDS];
Preferences preferences;

// Impostazioni configurabili (caricate da memoria)
int activeLeds = 143;
uint8_t currentBrightness = 100;
uint8_t gurgleIntensity = 150;
enum IdleMode { IDLE_OFF = 0, IDLE_SOLID = 1, IDLE_BREATHING = 2, IDLE_RAINBOW = 3 };
IdleMode idleMode = IDLE_BREATHING;
CRGB idleColor = CRGB::Yellow;
int maxCurrentMA = 500;

// ==================== GT7 CONFIG ====================
GT7_UDP_Parser gt7Telem;
Packet packetContent;
IPAddress playstationIP(255, 255, 255, 255);
char packetVersion = 'A';

// ==================== WEB SERVER & UDP ====================
WebServer server(80);
WiFiUDP ledDiscoveryUdp;

// ==================== STATO GLOBALE ====================
unsigned long lastFlashTime = 0;
bool flashState = false;
unsigned long lastDataTime = 0;
bool gt7Connected = false;

// Temi disponibili
enum Theme {
    THEME_DEFAULT = 0,
    THEME_F1_CENTER = 1,
    THEME_SUPERCAR = 2,
    THEME_SMOOTH_FADE = 3,
    THEME_F1_REVERSE = 4,
    THEME_DEFAULT_REVERSE = 5
};
Theme currentTheme = THEME_DEFAULT;

// Dati telemetria correnti (per la dashboard)
float currentRPM = 0;
float currentMaxRPM = 0;
float currentSpeed = 0;
uint8_t currentGear = 0;
float currentThrottle = 0;
float currentBrake = 0;

// ==================== PAGINA WEB ====================
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>GT7 LED Controller</title>
<style>
  * { margin:0; padding:0; box-sizing:border-box; }
  body { font-family:'Segoe UI',sans-serif; background:#0a0a0f; color:#e0e0e0; min-height:100vh; }
  .header { background:linear-gradient(135deg,#1a1a2e,#16213e); padding:20px; text-align:center; border-bottom:2px solid #e94560; }
  .header h1 { font-size:1.8em; background:linear-gradient(90deg,#e94560,#0f3460); -webkit-background-clip:text; -webkit-text-fill-color:transparent; }
  .header p { color:#888; margin-top:5px; }
  .container { max-width:600px; margin:20px auto; padding:0 15px; }
  .card { background:#12121a; border-radius:12px; padding:20px; margin-bottom:15px; border:1px solid #1a1a2e; }
  .card h2 { color:#e94560; font-size:1.1em; margin-bottom:15px; border-bottom:1px solid #1a1a2e; padding-bottom:8px; }
  .status-row { display:flex; justify-content:space-between; align-items:center; padding:8px 0; }
  .status-label { color:#888; }
  .status-value { font-weight:bold; font-size:1.1em; }
  .connected { color:#00e676; }
  .disconnected { color:#e94560; }
  .rpm-bar { width:100%; height:30px; background:#1a1a2e; border-radius:8px; overflow:hidden; margin:10px 0; }
  .rpm-fill { height:100%; border-radius:8px; transition:width 0.1s; }
  .gauge-grid { display:grid; grid-template-columns:1fr 1fr 1fr; gap:10px; text-align:center; }
  .gauge { background:#1a1a2e; border-radius:10px; padding:15px 10px; }
  .gauge .val { font-size:1.6em; font-weight:bold; color:#fff; }
  .gauge .lbl { font-size:0.75em; color:#888; margin-top:4px; }
  .slider-row { display:flex; align-items:center; gap:10px; margin-top:10px; }
  .slider-row input[type=range] { flex:1; accent-color:#e94560; }
  .slider-row span { min-width:40px; text-align:right; }
  select { width:100%; padding:10px; background:#1a1a2e; color:#fff; border:1px solid #e94560; border-radius:8px; font-size:1em; margin-top:10px; outline:none; }
  .btn { background:#e94560; color:#fff; border:none; padding:10px 20px; border-radius:8px; cursor:pointer; font-size:1em; width:100%; margin-top:10px; }
  .btn:hover { background:#c73050; }
</style>
</head>
<body>
<div class="header">
  <h1>🏎️ GT7 LED Controller</h1>
  <p>ESP32 S2 Mini &bull; WS2812B &bull; 143 LED</p>
</div>
<div class="container">

  <div class="card">
    <h2>📡 Stato Connessione</h2>
    <div class="status-row">
      <span class="status-label">Wi-Fi</span>
      <span class="status-value connected" id="wifi-status">Connesso</span>
    </div>
    <div class="status-row">
      <span class="status-label">IP</span>
      <span class="status-value" id="ip-addr">--</span>
    </div>
    <div class="status-row">
      <span class="status-label">Gran Turismo 7</span>
      <span class="status-value" id="gt7-status">--</span>
    </div>
  </div>

  <div class="card">
    <h2>🏁 Telemetria</h2>
    <div class="gauge-grid">
      <div class="gauge">
        <div class="val" id="rpm-val">0</div>
        <div class="lbl">RPM</div>
      </div>
      <div class="gauge">
        <div class="val" id="speed-val">0</div>
        <div class="lbl">KM/H</div>
      </div>
      <div class="gauge">
        <div class="val" id="gear-val">N</div>
        <div class="lbl">MARCIA</div>
      </div>
    </div>
    <div class="rpm-bar"><div class="rpm-fill" id="rpm-bar" style="width:0%;background:linear-gradient(90deg,#00e676,#ffeb3b,#e94560)"></div></div>
    <div class="gauge-grid" style="grid-template-columns:1fr 1fr;margin-top:10px">
      <div class="gauge">
        <div class="val" id="throttle-val">0%</div>
        <div class="lbl">ACCELERATORE</div>
      </div>
      <div class="gauge">
        <div class="val" id="brake-val">0%</div>
        <div class="lbl">FRENO</div>
      </div>
    </div>
  </div>

  <div class="card">
    <h2>🎨 Personalizzazione</h2>
    
    <div class="status-label" style="margin-top:10px;">Tema LED</div>
    <select id="theme-selector" onchange="setTheme()">
      <option value="0" %THEME_0_SEL%>Tema 1: Default (Verde/Giallo/Rosso)</option>
      <option value="5" %THEME_5_SEL%>Tema 6: Default Reverse (Da destra a sinistra)</option>
      <option value="1" %THEME_1_SEL%>Tema 2: Formula 1 (Dal centro)</option>
      <option value="4" %THEME_4_SEL%>Tema 5: F1 Reverse (Dai margini)</option>
      <option value="2" %THEME_2_SEL%>Tema 3: Supercar (Shift Light Blu)</option>
      <option value="3" %THEME_3_SEL%>Tema 4: Sfumatura (Ghiaccio-Fuoco)</option>
    </select>

    <div class="status-label" style="margin-top:15px;">Luminosità</div>
    <div class="slider-row">
      <input type="range" id="brightness" min="10" max="255" value="%BRIGHTNESS%">
      <span id="bright-val">%BRIGHTNESS%</span>
    </div>
    
    <div class="status-label" style="margin-top:15px;">Numero totale di LED fisici</div>
    <div class="slider-row">
      <input type="number" id="num-leds" style="width:100%; padding:10px; background:#1a1a2e; color:#fff; border:1px solid #e94560; border-radius:8px; font-size:1em; outline:none;" value="%ACTIVE_LEDS%" min="10" max="200">
    </div>

    <div class="status-label" style="margin-top:15px;">Intensità Gorgoglio (0 = Disattivato)</div>
    <div class="slider-row">
      <input type="range" id="gurgle-intensity" min="0" max="255" value="%GURGLE_VAL%">
      <span id="gurgle-val">%GURGLE_VAL%</span>
    </div>

    <div class="gauge-grid" style="grid-template-columns:1fr 1fr; margin-top:20px;">
      <button class="btn" style="background:#0f3460; margin-top:0;" onclick="updateSettings(false)">Applica al volo</button>
      <button class="btn" style="margin-top:0;" onclick="updateSettings(true)">Salva Preset Avvio</button>
    </div>
  </div>

  <div class="card">
    <h2>⚡ Energia e Limiti (Stile WLED)</h2>
    <div class="status-label" style="margin-top:15px;">Limite di Corrente (mA) - [Attento all'alimentatore!]</div>
    <div class="slider-row">
      <input type="number" id="max-ma" style="width:100%; padding:10px; background:#1a1a2e; color:#fff; border:1px solid #e94560; border-radius:8px; font-size:1em; outline:none;" value="%MAX_MA%" min="100" max="10000" step="100">
    </div>
  </div>

  <div class="card">
    <h2>💤 Modalità Standby (IDLE)</h2>
    <div class="status-label">Animazione a Gioco Spento</div>
    <select id="idle-mode" class="theme-select" onchange="updateSettings(false)">
      <option value="0" %IDLE_0_SEL%>Spento (Nessuna luce)</option>
      <option value="1" %IDLE_1_SEL%>Colore Fisso</option>
      <option value="2" %IDLE_2_SEL%>Respiro (Breathing)</option>
      <option value="3" %IDLE_3_SEL%>Arcobaleno Fluido</option>
    </select>
    
    <div class="status-label" style="margin-top:15px;">Colore Base Standby</div>
    <div class="slider-row" style="justify-content:left;">
      <input type="color" id="idle-color" value="%IDLE_COLOR%" style="width:100%; height:40px; background:none; border:none; cursor:pointer;" onchange="updateSettings(false)">
    </div>
  </div>

  <div class="card">
    <h2>🔧 Sistema</h2>
    <button class="btn" onclick="if(confirm('Riavviare?'))fetch('/restart')">Riavvia ESP32</button>
  </div>

</div>
<script>
function update(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    document.getElementById('wifi-status').textContent=d.wifi?'Connesso':'Disconnesso';
    document.getElementById('wifi-status').className='status-value '+(d.wifi?'connected':'disconnected');
    document.getElementById('ip-addr').textContent=d.ip;
    document.getElementById('gt7-status').textContent=d.gt7?'In Gara!':'In Attesa...';
    document.getElementById('gt7-status').className='status-value '+(d.gt7?'connected':'disconnected');
    document.getElementById('rpm-val').textContent=Math.round(d.rpm);
    document.getElementById('speed-val').textContent=Math.round(d.speed*3.6);
    var g=d.gear; document.getElementById('gear-val').textContent=g==0?'R':(g==15?'N':g);
    document.getElementById('throttle-val').textContent=Math.round(d.throttle/2.55)+'%';
    document.getElementById('brake-val').textContent=Math.round(d.brake/2.55)+'%';
    var pct=d.maxRpm>0?(d.rpm/d.maxRpm*100):0;
    document.getElementById('rpm-bar').style.width=Math.min(pct,100)+'%';
  }).catch(()=>{});
}
function updateSettings(save){
  var b=document.getElementById('brightness').value;
  var l=document.getElementById('num-leds').value;
  var g=document.getElementById('gurgle-intensity').value;
  var ma=document.getElementById('max-ma').value;
  var im=document.getElementById('idle-mode').value;
  var ic=document.getElementById('idle-color').value.replace('#','');
  fetch('/api/settings?b='+b+'&l='+l+'&g='+g+'&ma='+ma+'&im='+im+'&ic='+ic+'&save='+(save?1:0));
  if(save) alert("Preset salvato! La scheda si avvierà con queste impostazioni.");
}
function setTheme(){
  var t=document.getElementById('theme-selector').value;
  fetch('/api/theme?v='+t);
}
document.getElementById('brightness').oninput=function(){document.getElementById('bright-val').textContent=this.value;};
document.getElementById('gurgle-intensity').oninput=function(){document.getElementById('gurgle-val').textContent=this.value;};
document.getElementById('brightness').onchange=function(){updateSettings(false);};
document.getElementById('gurgle-intensity').onchange=function(){updateSettings(false);};
document.getElementById('num-leds').onchange=function(){updateSettings(false);};
document.getElementById('max-ma').onchange=function(){updateSettings(false);};
setInterval(update,200);
update();
</script>
</body>
</html>
)rawliteral";

// ==================== HANDLER WEB ====================
void handleRoot() {
    String html = FPSTR(DASHBOARD_HTML);
    
    char hexCol[8];
    sprintf(hexCol, "#%02x%02x%02x", idleColor.r, idleColor.g, idleColor.b);
    html.replace("%MAX_MA%", String(maxCurrentMA));
    html.replace("%IDLE_COLOR%", String(hexCol));
    html.replace("%IDLE_0_SEL%", idleMode == IDLE_OFF ? "selected" : "");
    html.replace("%IDLE_1_SEL%", idleMode == IDLE_SOLID ? "selected" : "");
    html.replace("%IDLE_2_SEL%", idleMode == IDLE_BREATHING ? "selected" : "");
    html.replace("%IDLE_3_SEL%", idleMode == IDLE_RAINBOW ? "selected" : "");

    html.replace("%BRIGHTNESS%", String(currentBrightness));
    html.replace("%ACTIVE_LEDS%", String(activeLeds));
    html.replace("%GURGLE_VAL%", String(gurgleIntensity));
    html.replace("%THEME_0_SEL%", currentTheme == THEME_DEFAULT ? "selected" : "");
    html.replace("%THEME_5_SEL%", currentTheme == THEME_DEFAULT_REVERSE ? "selected" : "");
    html.replace("%THEME_1_SEL%", currentTheme == THEME_F1_CENTER ? "selected" : "");
    html.replace("%THEME_4_SEL%", currentTheme == THEME_F1_REVERSE ? "selected" : "");
    html.replace("%THEME_2_SEL%", currentTheme == THEME_SUPERCAR ? "selected" : "");
    html.replace("%THEME_3_SEL%", currentTheme == THEME_SMOOTH_FADE ? "selected" : "");
    server.send(200, "text/html", html);
}

void handleApiStatus() {
    String json = "{";
    json += "\"wifi\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
    json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"gt7\":" + String(gt7Connected ? "true" : "false") + ",";
    json += "\"rpm\":" + String(currentRPM, 0) + ",";
    json += "\"maxRpm\":" + String(currentMaxRPM, 0) + ",";
    json += "\"speed\":" + String(currentSpeed, 2) + ",";
    json += "\"gear\":" + String(currentGear) + ",";
    json += "\"throttle\":" + String(currentThrottle) + ",";
    json += "\"brake\":" + String(currentBrake) + ",";
    json += "\"brightness\":" + String(currentBrightness) + ",";
    json += "\"theme\":" + String((int)currentTheme) + ",";
    json += "\"idleMode\":" + String((int)idleMode) + ",";
    json += "\"gurgle\":" + String(gurgleIntensity) + ",";
    // Letti dallo schermo GT7 per rimandarli invariati in /api/settings
    json += "\"leds\":" + String(activeLeds) + ",";
    json += "\"maxMa\":" + String(maxCurrentMA) + ",";
    json += "\"idleColor\":" + String(((uint32_t)idleColor.r << 16) | ((uint32_t)idleColor.g << 8) | idleColor.b);
    json += "}";
    server.send(200, "application/json", json);
}

void handleApiSettings() {
    if (server.hasArg("b") && server.hasArg("l") && server.hasArg("g") && server.hasArg("ma") && server.hasArg("im") && server.hasArg("ic")) {
        currentBrightness = server.arg("b").toInt();
        int newLeds = server.arg("l").toInt();
        gurgleIntensity = server.arg("g").toInt();
        maxCurrentMA = server.arg("ma").toInt();
        idleMode = (IdleMode)server.arg("im").toInt();
        
        String hex = server.arg("ic");
        long number = strtol(hex.c_str(), nullptr, 16);
        idleColor = CRGB(number >> 16, number >> 8 & 0xFF, number & 0xFF);
        
        if (idleMode > IDLE_RAINBOW) idleMode = IDLE_BREATHING;
        if (newLeds > 0 && newLeds <= MAX_LEDS) {
            // Spegne i LED oltre la nuova lunghezza, altrimenti restano accesi
            if (newLeds < activeLeds) fill_solid(leds + newLeds, activeLeds - newLeds, CRGB::Black);
            activeLeds = newLeds;
        }
        
        FastLED.setBrightness(currentBrightness);
        FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrentMA);
        
        if (server.hasArg("save") && server.arg("save") == "1") {
            preferences.putInt("leds", activeLeds);
            preferences.putInt("brightness", currentBrightness);
            preferences.putInt("gurgle", gurgleIntensity);
            preferences.putInt("max_ma", maxCurrentMA);
            preferences.putInt("idle_mode", (int)idleMode);
            preferences.putUInt("idle_col", (idleColor.r << 16) | (idleColor.g << 8) | idleColor.b);
        }
        
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing values");
    }
}

void handleApiTheme() {
    if (server.hasArg("v")) {
        int theme = server.arg("v").toInt();
        if (theme < THEME_DEFAULT || theme > THEME_DEFAULT_REVERSE) {
            server.send(400, "text/plain", "Invalid theme");
            return;
        }
        currentTheme = (Theme)theme;
        preferences.putInt("theme", (int)currentTheme);
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Missing value");
    }
}

void handleRestart() {
    server.send(200, "text/plain", "Riavvio...");
    delay(500);
    ESP.restart();
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println("=== GT7 LED Controller ===");
    Serial.println("Boot avviato!");

    // Carica preferenze salvate
    preferences.begin("gt7-led", false);
    activeLeds = preferences.getInt("leds", 150); // di default 150 se non salvato
    currentBrightness = preferences.getInt("brightness", 100);
    currentTheme = (Theme)preferences.getInt("theme", THEME_DEFAULT);
    gurgleIntensity = preferences.getInt("gurgle", 150);
    maxCurrentMA = preferences.getInt("max_ma", 500);
    idleMode = (IdleMode)preferences.getInt("idle_mode", IDLE_BREATHING);
    uint32_t cInt = preferences.getUInt("idle_col", 0xFFFF00); // Default Yellow
    idleColor = CRGB(cInt >> 16, (cInt >> 8) & 0xFF, cInt & 0xFF);

    // Inizializza FastLED
    Serial.println("Inizializzazione FastLED...");
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, MAX_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(currentBrightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrentMA);
    Serial.println("FastLED OK!");

    // Mostra blu durante il boot
    fill_solid(leds, activeLeds, CRGB::Blue);
    FastLED.show();

    // WiFiManager: crea AP "GT7_LED_Config" se non trova rete nota
    Serial.println("Avvio WiFiManager...");
    WiFiManager wm;
    wm.setConfigPortalTimeout(120); // Timeout 2 minuti
    bool res = wm.autoConnect("GT7_LED_Config");

    if (!res) {
        Serial.println("Wi-Fi timeout! Riavvio...");
        delay(3000);
        ESP.restart();
    }

    Serial.println("Connesso al Wi-Fi!");
    Serial.print("IP: "); Serial.println(WiFi.localIP());

    // Mostra verde
    fill_solid(leds, activeLeds, CRGB::Green);
    FastLED.show();
    delay(1000);

    // Avvia Web Server
    Serial.println("Avvio Web Server...");
    server.on("/", handleRoot);
    server.on("/api/status", handleApiStatus);
    server.on("/api/settings", handleApiSettings);
    server.on("/api/theme", handleApiTheme);
    server.on("/restart", handleRestart);
    server.begin();
    Serial.println("Web Server attivo su http://" + WiFi.localIP().toString());

    // Inizializza GT7 Telemetry
    Serial.println("Inizializzazione GT7 Telemetry...");
    gt7Telem.begin(playstationIP, packetVersion);
    gt7Telem.sendHeartbeat();
    Serial.println("Setup completato!");

    lastDataTime = millis();
}

// ==================== LOOP ====================
void loop() {
    // Gestisci richieste web
    server.handleClient();

    // Broadcast IP address for auto-discovery
    static unsigned long lastDiscoveryBcast = 0;
    if (millis() - lastDiscoveryBcast > 2000 && WiFi.status() == WL_CONNECTED) {
        ledDiscoveryUdp.beginPacket(IPAddress(255,255,255,255), 33741);
        String msg = "GT7LED|" + WiFi.localIP().toString();
        ledDiscoveryUdp.print(msg);
        ledDiscoveryUdp.endPacket();
        lastDiscoveryBcast = millis();
    }

    // Leggi telemetria GT7
    packetContent = gt7Telem.readData();

    currentRPM = packetContent.packetContent.EngineRPM;
    currentMaxRPM = packetContent.packetContent.maxAlertRPM;
    currentSpeed = packetContent.packetContent.speed;
    currentGear = gt7Telem.getCurrentGearFromByte();
    currentThrottle = packetContent.packetContent.throttle;
    currentBrake = packetContent.packetContent.brake;

    // Pacchetto valido?
    if (packetContent.packetContent.packetId != 0 && currentMaxRPM > 0) {
        lastDataTime = millis();
        gt7Connected = true;
    }

    bool isPaused = ((int16_t)packetContent.packetContent.flags & (1 << 1)) != 0;
    bool inGame = ((int16_t)packetContent.packetContent.flags & (1 << 0)) != 0; // CarOnTrack
    bool isLoading = ((int16_t)packetContent.packetContent.flags & (1 << 2)) != 0;

    // Modalità IDLE
    if (millis() - lastDataTime > 1000 || currentMaxRPM <= 0 || packetContent.packetContent.packetId == 0 || isPaused || !inGame || isLoading) {
        gt7Connected = false;
        
        switch (idleMode) {
            case IDLE_OFF:
                fill_solid(leds, activeLeds, CRGB::Black);
                break;
            case IDLE_SOLID:
                fill_solid(leds, activeLeds, idleColor);
                break;
            case IDLE_BREATHING: {
                uint8_t fadeVal = beatsin8(20, 20, 255);
                CRGB breathingColor = idleColor;
                breathingColor.nscale8(fadeVal);
                fill_solid(leds, activeLeds, breathingColor);
                break;
            }
            case IDLE_RAINBOW: {
                fill_rainbow(leds, activeLeds, millis() / 10, 255 / activeLeds);
                break;
            }
        }
        
        FastLED.show();

        // Heartbeat periodico
        static unsigned long lastHeartbeat = 0;
        if (millis() - lastHeartbeat > 10000) {
            gt7Telem.sendHeartbeat();
            lastHeartbeat = millis();
        }
        return;
    }

    // ==================== MODALITÀ GARA ====================
    float rpm = currentRPM;
    float maxRPM = currentMaxRPM;
    if (rpm > maxRPM) rpm = maxRPM;

    // Leggiamo il flag ufficiale di GT7 per sapere esattamente quando il gioco fa lampeggiare il limitatore
    bool revLimiterActive = ((int16_t)packetContent.packetContent.flags & (1 << 5)) != 0;

    if (revLimiterActive) {
        // Se il limitatore è attivo, tutti i temi fanno lampeggiare tutto di ROSSO
        if (millis() - lastFlashTime > 50) {
            flashState = !flashState;
            lastFlashTime = millis();
        }
        fill_solid(leds, activeLeds, flashState ? CRGB::Red : CRGB::Black);
    } else {
        // Logica specifica per ogni tema
        switch (currentTheme) {
            case THEME_DEFAULT: {
                int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
                for (int i = 0; i < activeLeds; i++) {
                    if (i < numLedsToLight) {
                        if (i < (activeLeds * 0.6)) leds[i] = CRGB::Green;
                        else if (i < (activeLeds * 0.85)) leds[i] = CRGB::Yellow;
                        else leds[i] = CRGB::Red;
                    } else {
                        leds[i] = CRGB::Black;
                    }
                }
                break;
            }
            case THEME_DEFAULT_REVERSE: {
                int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
                for (int i = 0; i < activeLeds; i++) {
                    if (i < numLedsToLight) {
                        if (i < (activeLeds * 0.6)) leds[activeLeds - 1 - i] = CRGB::Green;
                        else if (i < (activeLeds * 0.85)) leds[activeLeds - 1 - i] = CRGB::Yellow;
                        else leds[activeLeds - 1 - i] = CRGB::Red;
                    } else {
                        leds[activeLeds - 1 - i] = CRGB::Black;
                    }
                }
                break;
            }
            case THEME_F1_CENTER: {
                int center = activeLeds / 2;
                int ledsPerSide = (int)((rpm / maxRPM) * center);
                fill_solid(leds, activeLeds, CRGB::Black); // Pulisci
                for (int i = 0; i < ledsPerSide; i++) {
                    CRGB color;
                    if (i < center * 0.35) color = CRGB::Green;
                    else if (i < center * 0.75) color = CRGB::Yellow;
                    else color = CRGB::Red;
                    // Accendi simmetricamente dal centro verso i bordi
                    if (center + i < activeLeds) leds[center + i] = color;
                    if (center - 1 - i >= 0) leds[center - 1 - i] = color;
                }
                break;
            }
            case THEME_F1_REVERSE: {
                int center = activeLeds / 2;
                int ledsPerSide = (int)((rpm / maxRPM) * center);
                fill_solid(leds, activeLeds, CRGB::Black); // Pulisci
                for (int i = 0; i < ledsPerSide; i++) {
                    CRGB color;
                    if (i < center * 0.35) color = CRGB::Green;
                    else if (i < center * 0.75) color = CRGB::Yellow;
                    else color = CRGB::Red;
                    // Accendi simmetricamente dai bordi verso il centro
                    if (i < activeLeds) leds[i] = color; // Bordo sinistro verso il centro
                    if (activeLeds - 1 - i >= 0) leds[activeLeds - 1 - i] = color; // Bordo destro verso il centro
                }
                break;
            }
            case THEME_SUPERCAR: {
                fill_solid(leds, activeLeds, CRGB::Black);
                // Si accende solo dal 70% in poi
                float threshold = maxRPM * 0.70;
                if (rpm > threshold) {
                    float activeRange = maxRPM - threshold;
                    float currentActive = rpm - threshold;
                    int numLedsToLight = (int)((currentActive / activeRange) * activeLeds);
                    for (int i = 0; i < numLedsToLight; i++) {
                        leds[i] = CRGB::Blue;
                    }
                }
                break;
            }
            case THEME_SMOOTH_FADE: {
                // Sfumatura fluida per tutta la striscia (Azzurro ghiaccio -> Viola -> Rosso)
                float pct = rpm / maxRPM;
                // hue da 140 (azzurro) a 0 (rosso)
                uint8_t hue = 140 - (uint8_t)(pct * 140); 
                fill_solid(leds, activeLeds, CHSV(hue, 255, 255));
                break;
            }
        }

        // Effetto gorgoglio (punta tremolante dinamica)
        if (gurgleIntensity > 0 && rpm > 0) {
            int numGurgleLeds = (gurgleIntensity / 15) + 1; // fino a ~17 LED influenzati!
            
            static unsigned long lastGurgleTime = 0;
            static uint8_t flickerArray[20];
            
            // Rigenera i valori di flicker ogni 30ms (effetto meccanico non strobo)
            if (millis() - lastGurgleTime > 30) {
                for(int k=0; k<20; k++) {
                    flickerArray[k] = random8(255 - gurgleIntensity, 255);
                }
                lastGurgleTime = millis();
            }

            switch (currentTheme) {
                case THEME_DEFAULT: {
                    int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
                    for(int n=0; n<numGurgleLeds; n++){
                        int idx = numLedsToLight - 1 - n;
                        if (idx >= 0 && idx < activeLeds) {
                            CRGB color = (idx < activeLeds * 0.6) ? CRGB::Green : ((idx < activeLeds * 0.85) ? CRGB::Yellow : CRGB::Red);
                            color.nscale8(flickerArray[n % 20]);
                            leds[idx] = color;
                        }
                    }
                    break;
                }
                case THEME_DEFAULT_REVERSE: {
                    int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
                    for(int n=0; n<numGurgleLeds; n++){
                        int vIdx = numLedsToLight - 1 - n;
                        if (vIdx >= 0 && vIdx < activeLeds) {
                            CRGB color = (vIdx < activeLeds * 0.6) ? CRGB::Green : ((vIdx < activeLeds * 0.85) ? CRGB::Yellow : CRGB::Red);
                            color.nscale8(flickerArray[n % 20]);
                            leds[activeLeds - 1 - vIdx] = color;
                        }
                    }
                    break;
                }
                case THEME_F1_CENTER: {
                    int center = activeLeds / 2;
                    int ledsPerSide = (int)((rpm / maxRPM) * center);
                    for(int n=0; n<numGurgleLeds; n++){
                        int vIdx = ledsPerSide - 1 - n;
                        if (vIdx >= 0 && vIdx < center) {
                            CRGB color = (vIdx < center * 0.35) ? CRGB::Green : ((vIdx < center * 0.75) ? CRGB::Yellow : CRGB::Red);
                            CRGB cL = color; cL.nscale8(flickerArray[n % 20]);
                            CRGB cR = color; cR.nscale8(flickerArray[(n+1) % 20]);
                            if (center + vIdx < activeLeds) leds[center + vIdx] = cR;
                            if (center - 1 - vIdx >= 0) leds[center - 1 - vIdx] = cL;
                        }
                    }
                    break;
                }
                case THEME_F1_REVERSE: {
                    int center = activeLeds / 2;
                    int ledsPerSide = (int)((rpm / maxRPM) * center);
                    for(int n=0; n<numGurgleLeds; n++){
                        int vIdx = ledsPerSide - 1 - n;
                        if (vIdx >= 0 && vIdx < center) {
                            CRGB color = (vIdx < center * 0.35) ? CRGB::Green : ((vIdx < center * 0.75) ? CRGB::Yellow : CRGB::Red);
                            CRGB cL = color; cL.nscale8(flickerArray[n % 20]);
                            CRGB cR = color; cR.nscale8(flickerArray[(n+1) % 20]);
                            if (vIdx < activeLeds) leds[vIdx] = cL;
                            if (activeLeds - 1 - vIdx >= 0) leds[activeLeds - 1 - vIdx] = cR;
                        }
                    }
                    break;
                }
                case THEME_SUPERCAR: {
                    float threshold = maxRPM * 0.70;
                    if (rpm > threshold) {
                        int numLedsToLight = (int)(((rpm - threshold) / (maxRPM - threshold)) * activeLeds);
                        for(int n=0; n<numGurgleLeds; n++){
                            int idx = numLedsToLight - 1 - n;
                            if (idx >= 0 && idx < activeLeds) {
                                CRGB color = CRGB::Blue;
                                color.nscale8(flickerArray[n % 20]);
                                leds[idx] = color;
                            }
                        }
                    }
                    break;
                }
                case THEME_SMOOTH_FADE:
                    break;
            }
        }
    }

    FastLED.show();



    // Heartbeat periodico
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 10000) {
        gt7Telem.sendHeartbeat();
        lastHeartbeat = millis();
    }
}
