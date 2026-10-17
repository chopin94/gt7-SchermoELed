#include <Arduino.h>
#include <FastLED.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <FS.h>
#include <LittleFS.h>
#include <OtaWebUpdate.h>
#include <GT7UDPParser.h>
#include <F1UdpReceiver.h>
#include <Preferences.h>
#include "LedEffects.h"
#include "RaceTelemetry.h"
#include "EventStream.h"
#include "WebPage.h"
#include "DashPage.h"
#include "DashAssets.h"
// Circuiti salvati: lo stesso codice dello schermo (firmware/dashboard/src)
#include "../../dashboard/src/LittleFsStorage.h"

// ==================== FIRMWARE ====================
#define LED_FIRMWARE_VERSION "2.4.0"
// Nome in rete: http://gt7led.local (mDNS) e nome DHCP sul router
#define LED_HOSTNAME "gt7led"
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
// Ultimo pacchetto usato, di GT7 o di F1 25 (il cruscotto lo manda appena arriva)
int32_t lastPacketId = -1;
// Ultimo pacchetto di GT7: readData() ripete l'ultimo finché non ne arriva uno nuovo
int32_t lastGt7PacketId = -1;
unsigned long lastGt7PacketMs = 0;

// ==================== F1 25 ====================
// Ascoltato insieme a GT7 sulla porta 20777: in F1 25 Impostazioni >
// Telemetria, UDP attiva e trasmissione (broadcast) attiva, formato 2025.
F1UdpReceiver f1Telem;
// Le copie che lo schermo inoltra quando F1 25 manda i dati solo a lui
// (broadcast spento): usate solo se dal gioco non arriva niente qui. Sulla
// stessa porta lo schermo saluta ogni 2 s: la striscia gli inoltra ciò che
// riceve dal gioco, così in F1 25 basta l'indirizzo di uno dei due.
F1UdpReceiver f1Relay(F1UdpReceiver::RELAY_PORT);

// ==================== WEB SERVER & UDP ====================
OtaWebServer server(80);
OtaWebUpdate *firmwareUpdate = nullptr;
EventStream stream;     // pagina della striscia: 20 fotogrammi al secondo
EventStream dashStream; // cruscotto del telefono: uno per pacchetto di GT7
WiFiUDP ledDiscoveryUdp;
// Dopo l'esito di un aggiornamento la striscia lo mostra per un momento.
bool ledHold = false;
unsigned long ledHoldStart = 0;
unsigned long bootMs = 0;

// ==================== INDIRIZZO FISSO ====================
// Scelto dalla pagina Sistema, uno per rete (fino a 4): vale solo sulla rete
// con lo stesso nome, router e maschera di quando è stato scelto. Così due
// hotspot con lo stesso nome (un iPhone e un Android) non si confondono, e
// su una rete che cambia indirizzi a ogni accensione resta quello del router.
// Serve dove i nomi .local non funzionano e l'indirizzo cambia, come
// l'hotspot dell'iPhone, perché il cruscotto aggiunto alla Home lo ricorda.
static constexpr int FIXED_SLOTS = 4;
bool fixedAddressActive = false;

static String fixedKey(int slot, const char *name) { return String("fx") + slot + "_" + name; }

// Il posto dell'indirizzo fisso per la rete attuale, -1 se non c'è.
static int fixedSlot()
{
    for (int i = 0; i < FIXED_SLOTS; ++i)
        if (preferences.getUInt(fixedKey(i, "ip").c_str(), 0) &&
            preferences.getString(fixedKey(i, "ssid").c_str(), "") == WiFi.SSID() &&
            preferences.getUInt(fixedKey(i, "gw").c_str(), 0) == uint32_t(WiFi.gatewayIP()) &&
            preferences.getUInt(fixedKey(i, "mk").c_str(), 0) == uint32_t(WiFi.subnetMask()))
            return i;
    return -1;
}

static void clearFixedSlot(int slot)
{
    for (const char *name : {"ssid", "ip", "gw", "mk"}) preferences.remove(fixedKey(slot, name).c_str());
}

// Hotspot dell'iPhone: rete 172.20.10.0/28, gli indirizzi si danno da .2 in
// su in ordine di collegamento, quindi .14 è l'ultimo che l'iPhone usa.
static bool onIphoneHotspot()
{
    return WiFi.gatewayIP() == IPAddress(172, 20, 10, 1) && WiFi.subnetMask() == IPAddress(255, 255, 255, 240);
}

static IPAddress suggestedAddress()
{
    return onIphoneHotspot() ? IPAddress(172, 20, 10, 14) : WiFi.localIP();
}

// Dopo la connessione con DHCP: se per questa rete c'è un indirizzo fisso,
// passa a quello (il DHCP si ferma e l'indirizzo resta anche ai ricollegamenti).
static void applyFixedAddress()
{
    const int slot = fixedSlot();
    IPAddress ip;
    if (slot >= 0) ip = IPAddress(preferences.getUInt(fixedKey(slot, "ip").c_str(), 0));
    // Sull'hotspot dell'iPhone da sola, senza sceglierlo: l'icona del
    // cruscotto sulla Home si apre sempre (si spegne con "Torna automatico").
    else if (onIphoneHotspot() && !preferences.getBool("hot_off", false)) ip = suggestedAddress();
    else return;
    if (ip == WiFi.localIP())
    {
        fixedAddressActive = true;
        return;
    }
    if (WiFi.config(ip, WiFi.gatewayIP(), WiFi.subnetMask(), WiFi.dnsIP()))
    {
        fixedAddressActive = true;
        Serial.print("Indirizzo fisso: ");
        Serial.println(WiFi.localIP());
    }
    else Serial.println("Indirizzo fisso non applicato: resta quello del router");
}

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

// Il cruscotto del telefono e i suoi file (icone, font, video per tenere
// acceso lo schermo): non cambiano finché non cambia il firmware.
void handleDash()
{
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "text/html; charset=utf-8", DASH_PAGE, sizeof(DASH_PAGE) - 1);
}

void handleDashManifest()
{
    server.sendHeader("Cache-Control", "no-cache");
    server.send_P(200, "application/manifest+json", DASH_MANIFEST, sizeof(DASH_MANIFEST) - 1);
}

static void sendAsset(const char *type, const uint8_t *data, size_t size)
{
    server.sendHeader("Cache-Control", "public, max-age=604800");
    server.send_P(200, type, reinterpret_cast<PGM_P>(data), size);
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
    static char buffer[1600];
    JsonOut j(buffer, sizeof(buffer));
    j.beginObject();
    j.text("app", "gt7led"); // per la ricerca automatica del cruscotto
    j.flag("wifi", WiFi.status() == WL_CONNECTED);
    j.text("ip", WiFi.localIP().toString().c_str());
    j.text("host", LED_HOSTNAME ".local");
    j.text("ssid", WiFi.SSID().c_str());
    j.text("gw", WiFi.gatewayIP().toString().c_str());
    j.text("mask", WiFi.subnetMask().toString().c_str());
    j.flag("fixed", fixedAddressActive);
    int savedNetworks = 0;
    for (int i = 0; i < FIXED_SLOTS; ++i) savedNetworks += preferences.getUInt(fixedKey(i, "ip").c_str(), 0) ? 1 : 0;
    j.field("fixedNets", static_cast<long long>(savedNetworks));
    j.text("suggest", suggestedAddress().toString().c_str());
    j.key("gt7").boolean(race.racing(now));
    char f1State[64];
    (f1Telem.live(3000) || !f1Relay.live(3000) ? f1Telem : f1Relay).describe(f1State, sizeof(f1State));
    j.text("f1", f1State); // cosa arriva da F1 25: si vede subito un'impostazione sbagliata
    j.text("screen", f1Relay.peer() == IPAddress() ? "" : f1Relay.peer().toString().c_str());
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
    j.field("clients", static_cast<long long>(stream.count() + dashStream.count()));
    j.endObject();
    // La ricerca automatica del cruscotto la chiede da un altro indirizzo
    server.sendHeader("Access-Control-Allow-Origin", "*");
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
// manda sempre b, l, g, ma, im, ic e, con questa versione, anche il resto come la pagina web.
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
void handleDashEvents() { dashStream.accept(server); }

// /api/network?ip=a.b.c.d fissa l'indirizzo su questa rete, ?ip=auto la
// riporta all'indirizzo del router. La striscia si riavvia per usarlo.
void handleApiNetwork()
{
    const String value = server.arg("ip");
    int slot = fixedSlot();
    if (value == "auto")
    {
        if (slot >= 0) clearFixedSlot(slot);
        if (onIphoneHotspot()) preferences.putBool("hot_off", true);
    }
    else
    {
        IPAddress ip;
        const uint32_t mask = WiFi.subnetMask(), net = uint32_t(WiFi.localIP()) & mask;
        if (!ip.fromString(value) || (uint32_t(ip) & mask) != net || ip == WiFi.gatewayIP() ||
            (uint32_t(ip) & ~mask) == 0 || (uint32_t(ip) & ~mask) == ~mask)
        {
            server.send(400, "text/plain", "Indirizzo non valido per questa rete (" + WiFi.localIP().toString() + " / " +
                                               WiFi.subnetMask().toString() + ")");
            return;
        }
        // Il posto di questa rete, se no uno libero, se no il primo
        for (int i = 0; i < FIXED_SLOTS && slot < 0; ++i)
            if (!preferences.getUInt(fixedKey(i, "ip").c_str(), 0)) slot = i;
        if (slot < 0) slot = 0;
        preferences.putString(fixedKey(slot, "ssid").c_str(), WiFi.SSID());
        preferences.putUInt(fixedKey(slot, "ip").c_str(), uint32_t(ip));
        preferences.putUInt(fixedKey(slot, "gw").c_str(), uint32_t(WiFi.gatewayIP()));
        preferences.putUInt(fixedKey(slot, "mk").c_str(), mask);
        preferences.remove("hot_off");
    }
    server.send(200, "text/plain", "OK");
    delay(500);
    ESP.restart();
}

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

static void useSample(const Race::Packet &p)
{
    lastPacketId = p.packetId;
    race.update(p, millis());
    LedFx::Event event;
    while (race.takeEvent(event)) effects.trigger(event, millis());
}

// Legge tutti i pacchetti arrivati: l'analisi del giro li vuole tutti, anche
// quando una richiesta web ha fermato il ciclo per qualche decina di ms.
static void readTelemetry()
{
    for (int k = 0; k < 8; ++k)
    {
        const Packet packet = gt7Telem.readData();
        const PacketC &d = packet.packetContent;
        if (d.magic != 0x47375330 || d.packetId == lastGt7PacketId) break;
        lastGt7PacketId = d.packetId;
        lastGt7PacketMs = millis();
        Race::Packet p;
        copyPacket(d, gt7Telem.getCurrentGearFromByte(), gt7Telem.getSuggestedGearFromByte(), p);
        useSample(p);
    }
    f1Telem.setRelay(f1Relay.peer());
    F1Udp::Car car;
    for (int k = 0; k < 8 && f1Telem.poll(car); ++k)
    {
        Race::Packet p;
        Race::fromF1(car, p);
        useSample(p);
    }
    for (int k = 0; k < 8 && f1Relay.poll(car); ++k)
    {
        if (f1Telem.live(1500)) continue;
        Race::Packet p;
        Race::fromF1(car, p);
        useSample(p);
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
    wm.setHostname(LED_HOSTNAME);
    wm.setConfigPortalTimeout(120); // Timeout 2 minuti
    if (!wm.autoConnect("GT7_LED_Config"))
    {
        Serial.println("Wi-Fi timeout! Riavvio...");
        delay(3000);
        ESP.restart();
    }
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    // Wi-Fi sempre sveglio: con il risparmio energetico (attivo di default)
    // la scheda dorme tra un beacon e l'altro, perde le domande mDNS di
    // gt7led.local e ritarda pacchetti di GT7 e flussi verso il telefono.
    WiFi.setSleep(false);
    applyFixedAddress();

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
    server.on("/api/network", handleApiNetwork);
    // Cruscotto del telefono
    server.on("/cruscotto", handleDash);
    server.on("/api/dash", handleDashEvents);
    server.on("/manifest.webmanifest", handleDashManifest);
    server.on("/icon-192.png", [] { sendAsset("image/png", DASH_ICON_192, sizeof(DASH_ICON_192)); });
    server.on("/icon-512.png", [] { sendAsset("image/png", DASH_ICON_512, sizeof(DASH_ICON_512)); });
    server.on("/apple-touch-icon.png", [] { sendAsset("image/png", DASH_ICON_180, sizeof(DASH_ICON_180)); });
    server.on("/cruscotto/num-b.woff2", [] { sendAsset("font/woff2", DASH_FONT_BOLD, sizeof(DASH_FONT_BOLD)); });
    server.on("/cruscotto/num-s.woff2", [] { sendAsset("font/woff2", DASH_FONT_SEMIBOLD, sizeof(DASH_FONT_SEMIBOLD)); });
    server.on("/cruscotto/sveglia.mp4", [] { sendAsset("video/mp4", DASH_WAKE_MP4, sizeof(DASH_WAKE_MP4)); });
    server.on("/cruscotto/sveglia.webm", [] { sendAsset("video/webm", DASH_WAKE_WEBM, sizeof(DASH_WAKE_WEBM)); });
    const OtaWebUpdate::Config update = {"GT7 LED", LED_FIRMWARE_TAG, LED_FIRMWARE_VERSION, GT7_OTA_PASSWORD};
    firmwareUpdate = OtaWebUpdate::attach(server, update, showFirmwareUpdate);
    server.begin();
    Serial.println("Web Server attivo su http://" + WiFi.localIP().toString());
    if (MDNS.begin(LED_HOSTNAME))
    {
        MDNS.addService("http", "tcp", 80);
        Serial.println("Anche su http://" LED_HOSTNAME ".local (cruscotto: /cruscotto)");
    }
    // Sul Wi-Fi con la pagina di aggiornamento: questo firmware va bene.
    OtaWebUpdate::confirmRunningFirmware();

    gt7Telem.begin(playstationIP, packetVersion);
    gt7Telem.sendHeartbeat();
    f1Telem.begin();
    f1Relay.begin();
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

    // Telemetria in diretta. Pagina della striscia: 20 volte al secondo, i
    // colori 5. Cruscotto: a ogni pacchetto nuovo di GT7 (60 al secondo), 4
    // volte al secondo senza GT7 perché il telefono sappia di essere collegato.
    // Il fotogramma si scrive una volta sola anche se serve a tutti e due.
    static char frameJson[1600];
    static size_t frameLength = 0;
    static uint32_t frameAt = 0;
    static bool frameValid = false;
    auto frameNow = [&]() -> bool
    {
        if (!frameValid || frameAt != now)
        {
            JsonOut j(frameJson, sizeof(frameJson));
            race.writeFrame(j, now);
            frameValid = j.ok();
            frameLength = j.length();
            frameAt = now;
        }
        return frameValid;
    };
    if (dashStream.count() > 0)
    {
        static int32_t dashPacket = -1;
        static uint32_t lastDash = 0;
        const bool fresh = lastPacketId != dashPacket;
        if ((fresh && now - lastDash >= 14) || now - lastDash >= 250)
        {
            dashPacket = lastPacketId;
            lastDash = now;
            if (frameNow()) dashStream.send("t", frameJson, frameLength);
        }
    }
    if (stream.count() > 0)
    {
        static uint32_t lastStream = 0, lastColors = 0;
        static char buffer[1600];
        if (now - lastStream >= 50)
        {
            lastStream = now;
            if (frameNow()) stream.send("t", frameJson, frameLength);
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

    // Heartbeat periodico verso la PS5: ogni 10 s mentre GT7 trasmette,
    // ogni secondo quando tace. Va anche alla PS5 stessa (nota da GT7 o da
    // F1 25) e, finché GT7 tace, agli indirizzi della rete a turno:
    // l'hotspot dell'iPhone non porta il broadcast.
    static unsigned long lastHeartbeat = 0;
    const bool gt7Receiving = lastGt7PacketMs > 0 && millis() - lastGt7PacketMs < 3000;
    if (millis() - lastHeartbeat > (gt7Receiving ? 10000UL : 1000UL))
    {
        gt7Telem.sendHeartbeats(f1Telem.gameAddress(), gt7Receiving);
        lastHeartbeat = millis();
    }
}
