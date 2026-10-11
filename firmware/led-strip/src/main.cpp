#include <Arduino.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <OtaWebUpdate.h>
#include <GT7UDPParser.h>
#include <Preferences.h>
#include "LedEffects.h"
#include "RaceTelemetry.h"
#include "EventStream.h"
#include "WebPage.h"
// Circuiti salvati: lo stesso codice dello schermo (firmware/dashboard/src)
#include "../../dashboard/src/LittleFsStorage.h"

// ==================== FIRMWARE ====================
#define LED_FIRMWARE_VERSION "2.0.0"
// Nome controllato dall'aggiornamento via Wi-Fi: accetta solo un firmware.bin
// della striscia LED (vedi lib/OtaUpdate nel firmware dello schermo).
static const char LED_FIRMWARE_TAG[] = OTA_TAG_PREFIX "led-lolin-s2-mini";
// Password della pagina /update (utente "admin"), vuota = nessuna: vedi platformio.ini
#ifndef GT7_OTA_PASSWORD
#define GT7_OTA_PASSWORD ""
#endif

// ==================== CONFIGURAZIONE LED ====================
#define LED_PIN     7
#define MAX_LEDS    LedFx::MAX_LEDS
#define LED_TYPE    WS2812B
#define COLOR_ORDER GRB

CRGB leds[MAX_LEDS];
LedFx::Rgb frame[MAX_LEDS];
Preferences preferences;

// Impostazioni configurabili (caricate da memoria). Tema, animazione di
// riposo, colore, gorgoglio, avvisi e scala del delta stanno in effects.settings.
LedFx::Renderer effects;
int activeLeds = 143;
uint8_t currentBrightness = 100;
int maxCurrentMA = 500;
int idleOffMinutes = 0; // 0 = mai: la striscia resta accesa senza GT7

// ==================== GT7 ====================
GT7_UDP_Parser gt7Telem;
IPAddress playstationIP(255, 255, 255, 255);
// Pacchetto 'C', come lo schermo: tempo del giro in corso e pedali filtrati.
const char packetVersion = 'C';
Race::Telemetry race;
LittleFsStorage trackFiles;
TrackStore::Store trackStore(trackFiles);
bool trackFilesReady = false;
int32_t lastPacketId = -1;

// ==================== WEB SERVER & UDP ====================
OtaWebServer server(80);
OtaWebUpdate *firmwareUpdate = nullptr;
EventStream stream;
WiFiUDP ledDiscoveryUdp;
// Dopo l'esito di un aggiornamento la striscia lo mostra per un momento.
bool ledHold = false;
unsigned long ledHoldStart = 0;
unsigned long bootMs = 0;

// ==================== IMPOSTAZIONI ====================
static void applyLayout(const LedFx::Layout &layout)
{
    const int previous = activeLeds;
    effects.setLayout(layout);
    activeLeds = effects.count();
    // Spegne i LED oltre la nuova lunghezza, altrimenti restano accesi
    if (activeLeds < previous) fill_solid(leds + activeLeds, previous - activeLeds, CRGB::Black);
}

static void saveSettings()
{
    const LedFx::Settings &s = effects.settings;
    const LedFx::Layout &l = effects.layout();
    preferences.putInt("leds", activeLeds);
    preferences.putInt("brightness", currentBrightness);
    preferences.putInt("gurgle", s.gurgle);
    preferences.putInt("max_ma", maxCurrentMA);
    preferences.putInt("idle_mode", s.idleMode);
    preferences.putUInt("idle_col", (uint32_t(s.idleColor.r) << 16) | (uint32_t(s.idleColor.g) << 8) | s.idleColor.b);
    preferences.putInt("seg_r", l.right);
    preferences.putInt("seg_t", l.top);
    preferences.putInt("seg_l", l.left);
    preferences.putBool("start_l", l.startLeft);
    preferences.putUInt("alerts", s.alerts);
    preferences.putInt("delta_sc", static_cast<int>(s.deltaScaleS * 10 + 0.5f));
    preferences.putInt("idle_off", idleOffMinutes);
}

static void loadSettings()
{
    LedFx::Settings &s = effects.settings;
    activeLeds = constrain(preferences.getInt("leds", 150), 1, MAX_LEDS); // di default 150 se non salvato
    currentBrightness = preferences.getInt("brightness", 100);
    const int theme = preferences.getInt("theme", LedFx::THEME_DEFAULT);
    s.theme = theme >= 0 && theme < LedFx::THEME_COUNT ? theme : LedFx::THEME_DEFAULT;
    s.gurgle = preferences.getInt("gurgle", 150);
    maxCurrentMA = preferences.getInt("max_ma", 500);
    const int idle = preferences.getInt("idle_mode", LedFx::IDLE_BREATHING);
    s.idleMode = idle >= 0 && idle < LedFx::IDLE_COUNT ? idle : LedFx::IDLE_BREATHING;
    const uint32_t c = preferences.getUInt("idle_col", 0xFFFF00); // Default Yellow
    s.idleColor = LedFx::Rgb(c >> 16, (c >> 8) & 0xFF, c & 0xFF);
    s.alerts = preferences.getUInt("alerts", LedFx::ALERTS_DEFAULT) & LedFx::ALERTS_ALL;
    s.deltaScaleS = constrain(preferences.getInt("delta_sc", 10), 1, 50) / 10.0f;
    idleOffMinutes = constrain(preferences.getInt("idle_off", 0), 0, 240);
    // Montaggio: senza valori salvati, proporzioni di una TV 16:9 (lati a destra e sinistra, sopra)
    LedFx::Layout layout = LedFx::Layout::forCount(activeLeds);
    if (preferences.isKey("seg_r"))
    {
        layout.right = preferences.getInt("seg_r", layout.right);
        layout.top = preferences.getInt("seg_t", layout.top);
        layout.left = preferences.getInt("seg_l", layout.left);
        if (!layout.valid() || layout.count() != activeLeds) layout = LedFx::Layout::forCount(activeLeds);
    }
    layout.startLeft = preferences.getBool("start_l", false);
    effects.setLayout(layout);
    activeLeds = effects.count();
}

// ==================== HANDLER WEB ====================
void handleRoot()
{
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", WEB_PAGE, sizeof(WEB_PAGE) - 1);
}

static void sendJson(JsonOut &j)
{
    server.sendHeader("Cache-Control", "no-store");
    server.send(j.ok() ? 200 : 500, "application/json", j.c_str());
}

void handleApiStatus()
{
    const LedFx::Settings &s = effects.settings;
    const LedFx::Layout &l = effects.layout();
    const Race::Packet &p = race.packet();
    const uint32_t now = millis();
    static char buffer[1024];
    JsonOut j(buffer, sizeof(buffer));
    j.beginObject();
    j.flag("wifi", WiFi.status() == WL_CONNECTED);
    j.text("ip", WiFi.localIP().toString().c_str());
    j.key("gt7").boolean(race.racing(now));
    j.field("rpm", race.live(now) ? p.rpm : 0.0f, 0);
    j.field("maxRpm", race.live(now) ? p.maxAlertRpm : 0.0f, 0);
    j.field("speed", race.live(now) ? p.speed : 0.0f, 2);
    j.field("gear", static_cast<long long>(p.gear));
    j.field("throttle", static_cast<long long>(p.throttle));
    j.field("brake", static_cast<long long>(p.brake));
    // Letti dallo schermo GT7 per rimandarli invariati in /api/settings
    j.field("brightness", static_cast<long long>(currentBrightness));
    j.field("theme", static_cast<long long>(s.theme));
    j.field("idleMode", static_cast<long long>(s.idleMode));
    j.field("gurgle", static_cast<long long>(s.gurgle));
    j.field("leds", static_cast<long long>(activeLeds));
    j.field("maxMa", static_cast<long long>(maxCurrentMA));
    j.field("idleColor", static_cast<long long>((uint32_t(s.idleColor.r) << 16) | (uint32_t(s.idleColor.g) << 8) | s.idleColor.b));
    // Dalla versione 2: quanti temi e animazioni (lo schermo li scorre), avvisi, montaggio
    j.field("themes", static_cast<long long>(LedFx::THEME_COUNT));
    j.field("idleModes", static_cast<long long>(LedFx::IDLE_COUNT));
    j.field("alerts", static_cast<long long>(s.alerts));
    j.field("right", static_cast<long long>(l.right));
    j.field("top", static_cast<long long>(l.top));
    j.field("left", static_cast<long long>(l.left));
    j.flag("startLeft", l.startLeft);
    j.field("deltaScale", static_cast<long long>(s.deltaScaleS * 10 + 0.5f));
    j.field("idleOff", static_cast<long long>(idleOffMinutes));
    j.text("fw", LED_FIRMWARE_VERSION);
    j.field("rssi", static_cast<long long>(WiFi.RSSI()));
    j.field("uptime", static_cast<long long>((now - bootMs) / 1000));
    j.field("heap", static_cast<long long>(ESP.getFreeHeap()));
    j.field("psram", static_cast<long long>(ESP.getFreePsram()));
    j.field("tracks", static_cast<long long>(trackFilesReady ? trackStore.trackCount() : -1));
    j.field("clients", static_cast<long long>(stream.count()));
    j.endObject();
    sendJson(j);
}

static bool argInt(const char *name, int lo, int hi, int &out)
{
    if (!server.hasArg(name)) return false;
    const long v = server.arg(name).toInt();
    if (v < lo || v > hi) return false;
    out = static_cast<int>(v);
    return true;
}

// Ogni valore è facoltativo: quelli assenti restano com'erano. Lo schermo
// manda sempre b, l, g, ma, im, ic; la pagina web anche il resto.
void handleApiSettings()
{
    LedFx::Settings &s = effects.settings;
    int v;
    if (argInt("b", 0, 255, v)) currentBrightness = v;
    if (argInt("g", 0, 255, v)) s.gurgle = v;
    if (argInt("ma", 100, 10000, v)) maxCurrentMA = v;
    if (argInt("im", 0, 255, v)) s.idleMode = v < LedFx::IDLE_COUNT ? v : LedFx::IDLE_BREATHING;
    if (server.hasArg("ic"))
    {
        const long number = strtol(server.arg("ic").c_str(), nullptr, 16);
        s.idleColor = LedFx::Rgb(number >> 16, (number >> 8) & 0xFF, number & 0xFF);
    }
    if (argInt("al", 0, LedFx::ALERTS_ALL, v)) s.alerts = v;
    if (argInt("ds", 1, 50, v)) s.deltaScaleS = v / 10.0f;
    if (argInt("it", 0, 240, v)) idleOffMinutes = v;

    LedFx::Layout layout = effects.layout();
    int r, t, l;
    if (argInt("zr", 0, MAX_LEDS, r) && argInt("zt", 0, MAX_LEDS, t) && argInt("zl", 0, MAX_LEDS, l))
    {
        layout.right = r; layout.top = t; layout.left = l;
    }
    else if (argInt("l", 1, MAX_LEDS, v) && v != layout.count())
    {
        layout = layout.resized(v);
    }
    if (argInt("sl", 0, 1, v)) layout.startLeft = v != 0;
    if (!layout.valid())
    {
        server.send(400, "text/plain", "Numero di LED non valido (1-200 in tutto)");
        return;
    }
    if (layout.count() != effects.count() || layout.right != effects.layout().right ||
        layout.top != effects.layout().top || layout.startLeft != effects.layout().startLeft)
        applyLayout(layout);

    FastLED.setBrightness(currentBrightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrentMA);
    if (server.hasArg("save") && server.arg("save") == "1") saveSettings();
    server.send(200, "text/plain", "OK");
}

void handleApiTheme()
{
    if (!server.hasArg("v"))
    {
        server.send(400, "text/plain", "Missing value");
        return;
    }
    const int theme = server.arg("v").toInt();
    if (theme < 0 || theme >= LedFx::THEME_COUNT)
    {
        server.send(400, "text/plain", "Invalid theme");
        return;
    }
    effects.settings.theme = theme;
    preferences.putInt("theme", theme);
    server.send(200, "text/plain", "OK");
}

// Prova del montaggio: ogni lato nel suo colore per 15 secondi.
void handleApiTest()
{
    if (server.arg("layout") == "0") effects.stopLayoutTest();
    else effects.startLayoutTest(millis());
    server.send(200, "text/plain", "OK");
}

void handleApiSession()
{
    const size_t capacity = 16384; // mappa (fino a 512 punti), giri, frenate
    char *buffer = static_cast<char *>(malloc(capacity));
    if (!buffer)
    {
        server.send(503, "text/plain", "Memoria esaurita");
        return;
    }
    JsonOut j(buffer, capacity);
    race.writeSession(j);
    sendJson(j);
    free(buffer);
}

void handleApiReset()
{
    race.resetSession();
    server.send(200, "text/plain", "OK");
}

void handleEvents() { stream.accept(server); }

void handleRestart()
{
    server.send(200, "text/plain", "Riavvio...");
    delay(500);
    ESP.restart();
}

// ==================== AGGIORNAMENTO VIA WI-FI ====================
// Dopo un aggiornamento il firmware nuovo è in prova: viene confermato quando
// torna sul Wi-Fi con la pagina /update (fine di setup()). Se si blocca prima,
// al riavvio successivo torna da solo quello precedente.
extern "C" bool verifyRollbackLater() { return true; }

// Avanzamento sulla striscia stessa: barra blu che cresce, tutta verde alla
// fine, rossa per un momento se l'aggiornamento non riesce.
static void showFirmwareUpdate(const OtaImage::Progress &progress, void *)
{
    static int shownLeds = -1;
    if (progress.phase == OtaImage::Phase::Receiving)
    {
        const int lit = progress.total ? int(uint64_t(activeLeds) * progress.received / progress.total) : 0;
        // Pochi aggiornamenti bastano: ognuno occupa la striscia qualche ms.
        if (shownLeds >= 0 && lit < shownLeds + max(1, activeLeds / 20)) return;
        shownLeds = lit;
        fill_solid(leds, activeLeds, CRGB::Black);
        fill_solid(leds, lit, CRGB(0, 60, 255));
    }
    else
    {
        shownLeds = -1;
        fill_solid(leds, activeLeds, progress.phase == OtaImage::Phase::Done ? CRGB::Green : CRGB::Red);
        ledHold = true;
        ledHoldStart = millis();
    }
    FastLED.show();
}

// ==================== TELEMETRIA GT7 ====================
// Copia i campi usati dalla striscia dal pacchetto decifrato.
static void copyPacket(const PacketC &d, uint8_t gear, uint8_t suggested, Race::Packet &p)
{
    p.packetId = d.packetId;
    p.flags = static_cast<uint16_t>(static_cast<int16_t>(d.flags));
    p.rpm = d.EngineRPM;
    p.minAlertRpm = d.minAlertRPM;
    p.maxAlertRpm = d.maxAlertRPM;
    p.speed = d.speed;
    p.gear = gear;
    p.suggestedGear = suggested;
    p.throttle = d.throttle;
    p.brake = d.brake;
    p.throttleOut = d.throttleFiltered;
    p.brakeOut = d.brakeFiltered;
    p.clutch = d.clutch;
    for (int i = 0; i < 3; ++i)
    {
        p.position[i] = d.position[i];
        p.velocity[i] = d.worldVelocity[i];
    }
    p.fuelLevel = d.fuelLevel;
    p.fuelCapacity = d.fuelCapacity;
    p.boost = d.boost;
    p.oilPressure = d.oilPressure;
    p.waterTemp = d.waterTemp;
    p.oilTemp = d.oilTemp;
    for (int i = 0; i < 4; ++i)
    {
        p.tyreTemp[i] = d.tyreTemp[i];
        p.wheelRps[i] = d.wheelRPS[i];
        p.tyreRadius[i] = d.tyreRadius[i];
        p.suspHeight[i] = d.suspHeight[i];
    }
    p.lapCount = d.lapCount;
    p.totalLaps = d.totalLaps;
    p.bestLapMs = d.bestLaptime;
    p.lastLapMs = d.lastLaptime;
    p.currentLapMs = d.currentLap;
    p.dayMs = d.dayProgression;
    p.startPosition = d.RaceStartPosition;
    p.cars = d.preRaceNumCars;
    p.carCode = d.carCode;
}

// Legge tutti i pacchetti arrivati: l'analisi del giro li vuole tutti, anche
// quando una richiesta web ha fermato il ciclo per qualche decina di ms.
static void readTelemetry()
{
    for (int k = 0; k < 8; ++k)
    {
        const Packet packet = gt7Telem.readData();
        const PacketC &d = packet.packetContent;
        if (d.magic != 0x47375330 || d.packetId == lastPacketId) return;
        lastPacketId = d.packetId;
        Race::Packet p;
        copyPacket(d, gt7Telem.getCurrentGearFromByte(), gt7Telem.getSuggestedGearFromByte(), p);
        race.update(p, millis());
        LedFx::Event event;
        while (race.takeEvent(event)) effects.trigger(event, millis());
    }
}

// ==================== SETUP ====================
void setup()
{
    Serial.begin(115200);
    delay(2000);
    bootMs = millis();

    Serial.println("=== GT7 LED Controller " LED_FIRMWARE_VERSION " ===");

    // Carica preferenze salvate
    preferences.begin("gt7-led", false);
    loadSettings();

    // Inizializza FastLED
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, MAX_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(currentBrightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(5, maxCurrentMA);

    // Mostra blu durante il boot
    fill_solid(leds, activeLeds, CRGB::Blue);
    FastLED.show();

    // Circuiti salvati (la prima volta formatta la partizione: qualche secondo)
    trackFilesReady = trackFiles.begin();
    if (trackFilesReady)
    {
        trackStore.begin();
        race.setTrackStore(&trackStore);
        Serial.printf("Circuiti salvati: %d\n", trackStore.trackCount());
    }
    else Serial.println("LittleFS non disponibile: i circuiti non vengono salvati");

    // WiFiManager: crea AP "GT7_LED_Config" se non trova rete nota
    WiFiManager wm;
    wm.setConfigPortalTimeout(120); // Timeout 2 minuti
    if (!wm.autoConnect("GT7_LED_Config"))
    {
        Serial.println("Wi-Fi timeout! Riavvio...");
        delay(3000);
        ESP.restart();
    }
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // Mostra verde
    fill_solid(leds, activeLeds, CRGB::Green);
    FastLED.show();
    delay(1000);

    // Avvia Web Server
    server.on("/", handleRoot);
    server.on("/api/status", handleApiStatus);
    server.on("/api/settings", handleApiSettings);
    server.on("/api/theme", handleApiTheme);
    server.on("/api/test", handleApiTest);
    server.on("/api/session", handleApiSession);
    server.on("/api/reset", handleApiReset);
    server.on("/api/events", handleEvents);
    server.on("/restart", handleRestart);
    const OtaWebUpdate::Config update = {"GT7 LED", LED_FIRMWARE_TAG, LED_FIRMWARE_VERSION, GT7_OTA_PASSWORD};
    firmwareUpdate = OtaWebUpdate::attach(server, update, showFirmwareUpdate);
    server.begin();
    Serial.println("Web Server attivo su http://" + WiFi.localIP().toString());
    // Sul Wi-Fi con la pagina di aggiornamento: questo firmware va bene.
    OtaWebUpdate::confirmRunningFirmware();

    gt7Telem.begin(playstationIP, packetVersion);
    gt7Telem.sendHeartbeat();
}

// ==================== LOOP ====================
void loop()
{
    server.handleClient();
    firmwareUpdate->loop();
    if (ledHold)
    {
        if (millis() - ledHoldStart < 2000) return;
        ledHold = false;
    }

    // Annuncio in rete per lo schermo
    static unsigned long lastDiscoveryBcast = 0;
    if (millis() - lastDiscoveryBcast > 2000 && WiFi.status() == WL_CONNECTED)
    {
        ledDiscoveryUdp.beginPacket(IPAddress(255, 255, 255, 255), 33741);
        String msg = "GT7LED|" + WiFi.localIP().toString();
        ledDiscoveryUdp.print(msg);
        ledDiscoveryUdp.endPacket();
        lastDiscoveryBcast = millis();
    }

    readTelemetry();
    const uint32_t now = millis();

    // Un fotogramma ogni 12 ms al massimo (la striscia ne impiega circa 5 a riceverlo)
    static uint32_t lastFrame = 0;
    if (now - lastFrame >= 12)
    {
        lastFrame = now;
        LedFx::Input input;
        race.fillInput(input, now);
        effects.render(input, now, frame);
        // Spegnimento automatico: nessun pacchetto di GT7 da idleOffMinutes minuti
        const uint32_t quietSince = race.everReceived() ? race.lastPacketTime() : bootMs;
        const bool sleeping = idleOffMinutes > 0 && !input.racing && !effects.layoutTestActive(now) &&
                              now - quietSince > uint32_t(idleOffMinutes) * 60000u;
        for (int i = 0; i < activeLeds; ++i)
            leds[i] = sleeping ? CRGB::Black : CRGB(frame[i].r, frame[i].g, frame[i].b);
        FastLED.show();
    }

    // Telemetria in diretta per la pagina web: 20 volte al secondo, i colori 5
    if (stream.count() > 0)
    {
        static uint32_t lastStream = 0, lastColors = 0;
        static char buffer[1600];
        if (now - lastStream >= 50)
        {
            lastStream = now;
            JsonOut j(buffer, sizeof(buffer));
            race.writeFrame(j, now);
            if (j.ok()) stream.send("t", j.c_str(), j.length());
        }
        if (now - lastColors >= 200)
        {
            lastColors = now;
            static const char hex[] = "0123456789abcdef";
            int n = 0;
            for (int i = 0; i < activeLeds && n + 6 < static_cast<int>(sizeof(buffer)); ++i)
            {
                const uint8_t c[3] = {frame[i].r, frame[i].g, frame[i].b};
                for (int k = 0; k < 3; ++k)
                {
                    buffer[n++] = hex[c[k] >> 4];
                    buffer[n++] = hex[c[k] & 15];
                }
            }
            buffer[n] = 0;
            stream.send("leds", buffer, n);
        }
    }

    // Heartbeat verso la PS5. GT7 smette di trasmettere dopo un centinaio di
    // pacchetti (meno di 2 s) senza heartbeat: uno al secondo, come fa lo
    // schermo, altrimenti la striscia si ferma a scatti quando lo schermo è spento.
    static unsigned long lastHeartbeat = 0;
    if (millis() - lastHeartbeat > 1000)
    {
        gt7Telem.sendHeartbeat();
        lastHeartbeat = millis();
    }
}
