// Renders every dashboard screen of firmware/dashboard to 320x240 PNG files,
// running the real drawing code (themes, menus, waiting screen) on a PC.
// Build and run with tools/screenshots/build.sh.
#include <Arduino.h>
#include <WiFi.h>
#include <zlib.h>
#include <functional>
#include <string>
#include <vector>
#include "track_sim.h"

uint32_t g_hostMillis = 1000;
HostSerial Serial;
HostWiFi WiFi;

#define INCLUDE_GT7_WIFI true
#include <GT7UDPParser.h>
#include "ACUdpTelemetry.h"
GT7_UDP_Parser gt7Telem;
Packet gt7Packet;
ACUdpTelemetry acTelem;

// SimHub USB serial (FlowSerial in main.cpp) is not used for screenshots.
#define FlowSerialAvailable() 0
#define FlowSerialTimedRead() -1
#define FlowSerialWrite(data) ((void)(data))

#define private public
#define protected public
#include "SHCustomProtocol.h"
#undef private
#undef protected

static std::string g_outDir = "screenshots";

// ---------------------------------------------------------------- PNG output
static void putU32(std::vector<uint8_t> &v, uint32_t x)
{
    for (int i = 3; i >= 0; --i) v.push_back((x >> (i * 8)) & 0xFF);
}
static void chunk(FILE *f, const char *type, const std::vector<uint8_t> &data)
{
    std::vector<uint8_t> buf;
    putU32(buf, data.size());
    buf.insert(buf.end(), type, type + 4);
    buf.insert(buf.end(), data.begin(), data.end());
    uint32_t crc = crc32(0, buf.data() + 4, buf.size() - 4);
    putU32(buf, crc);
    fwrite(buf.data(), 1, buf.size(), f);
}
static void savePng(const std::string &name)
{
    const int w = tft.width(), h = tft.height();
    std::vector<uint8_t> raw;
    raw.reserve((w * 3 + 1) * h);
    for (int y = 0; y < h; ++y)
    {
        raw.push_back(0);
        for (int x = 0; x < w; ++x)
        {
            const uint16_t c = tft.readPixel(x, y); // RGB565
            raw.push_back(((c >> 11) & 0x1F) * 255 / 31);
            raw.push_back(((c >> 5) & 0x3F) * 255 / 63);
            raw.push_back((c & 0x1F) * 255 / 31);
        }
    }
    uLongf zlen = compressBound(raw.size());
    std::vector<uint8_t> z(zlen);
    compress2(z.data(), &zlen, raw.data(), raw.size(), 9);
    z.resize(zlen);

    const std::string path = g_outDir + "/" + name + ".png";
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) { fprintf(stderr, "cannot write %s\n", path.c_str()); exit(1); }
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    putU32(ihdr, w); putU32(ihdr, h);
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0}); // 8 bit RGB
    chunk(f, "IHDR", ihdr);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    fclose(f);
    printf("  %s\n", path.c_str());
}

// ---------------------------------------------------------------- scenarios
static SHCustomProtocol dash;
static TrackSim track;

// The simulated circuit, about 4.3 km, drawn clockwise.
static void buildTrack()
{
    track.build({{0, 0}, {420, 0}, {600, 30}, {660, 170}, {560, 300}, {600, 430}, {770, 480},
                 {830, 620}, {720, 730}, {560, 650}, {430, 520}, {270, 560}, {130, 700},
                 {-60, 650}, {-150, 470}, {-90, 300}, {-170, 170}, {-120, 40}});
    track.profile(80.0f, 13.5f, 13.0f, 8.5f);
}

// A driver lapping the circuit at 60 Hz like GT7 sends its packets, feeding
// the real lap analysis of the dashboard.
struct SimDriver
{
    float s = 0;             // metres driven since the start of the simulation
    uint32_t packet = 100000;
    int lapCount = 0;
    double lapStart = 0, time = 0;
    int32_t lastLapMs = -1;
    bool lapStarted = false;
    float speed = 0, accel = 0;
    float fuel = 0.62f;

    LapAnalysis::Sample sample() const
    {
        LapAnalysis::Sample out;
        out.timeMs = static_cast<uint32_t>(static_cast<int64_t>(packet) * 50 / 3);
        out.speed = speed;
        const TrackSim::Node n = track.at(s);
        out.hasPosition = true;
        out.x = n.x; out.z = n.z;
        out.hasVelocity = true;
        out.vx = n.hx * speed; out.vz = n.hz * speed;
        out.lapCount = lapCount;
        out.totalLaps = 10;
        out.lapTimeMs = lapStarted ? static_cast<int32_t>(llround((time - lapStart) * 1000.0)) : 0;
        out.lastLapMs = lastLapMs;
        out.throttle = accel > 0.3f ? fminf(1.0f, 0.35f + accel / 8.0f) : accel < -1.0f ? 0.0f : 0.25f;
        out.brake = accel < -1.0f ? fminf(1.0f, -accel / 13.0f) : 0.0f;
        out.fuelFraction = fuel;
        return out;
    }

    // One packet; `target` is the speed the driver wants (m/s).
    void step(float target)
    {
        const float dt = 1.0f / 60.0f;
        const float before = speed;
        if (target > speed) speed = fminf(target, speed + 9.5f * (1.0f - speed / 92.0f) * dt);
        else speed = fmaxf(target, speed - 13.0f * dt);
        accel = (speed - before) / dt;
        const float prev = s;
        s += speed * dt;
        time += dt;
        packet++;
        fuel -= 0.000012f;
        if (floorf(prev / track.length) != floorf(s / track.length))
        {
            const double crossing = time - (fmodf(s, track.length)) / fmaxf(speed, 1.0f);
            if (lapStarted) lastLapMs = static_cast<int32_t>(llround((crossing - lapStart) * 1000.0));
            lapStart = crossing;
            lapStarted = true;
            lapCount++;
        }
        g_hostMillis += 17;
        dash.lapAnalysis.update(sample());
    }

    // Laps the circuit with a pace for each third of the lap until `until`
    // returns true.
    void drive(const float pace[3], const std::function<bool()> &until)
    {
        while (!until())
        {
            const float fraction = fmodf(s, track.length) / track.length;
            const float p = pace[fraction < 1.0f / 3 ? 0 : fraction < 2.0f / 3 ? 1 : 2];
            step(track.speedAt(s + speed * 0.25f) * p);
        }
    }
};
static SimDriver driver;

static float lapFraction() { return fmodf(driver.s, track.length) / track.length; }

// Dashboard state at this moment of the simulation, as readGT7Wifi builds it.
static DashboardState simState()
{
    DashboardState st = dash.themePreviewData;
    const float kmh = driver.speed * 3.6f;
    static const float shift[] = {0, 62, 98, 132, 166, 200, 236, 400};
    int gear = 1;
    while (gear < 7 && kmh > shift[gear]) gear++;
    const float low = shift[gear - 1] * 0.62f, high = shift[gear];
    const int rpm = constrain(static_cast<int>(50 + 50 * (kmh - low) / (high - low)), 30, 99);
    st.gear = String(gear);
    st.speed = String(static_cast<int>(kmh));
    st.rpmPercent = rpm;
    st.engineRpm = 8000 * rpm / 100;
    st.rpmRedLineSetting = 90;
    st.rpmAlertRangeValid = true;
    st.revLimitAlertActive = false;
    st.suggestedGear = "";
    const LapAnalysis::Sample now = driver.sample();
    st.currentLapTime = dash.formatLapTimeMs(now.lapTimeMs);
    st.lastLapTime = dash.formatLapTimeMs(driver.lastLapMs);
    st.bestLapTime = dash.formatLapTimeMs(dash.lapAnalysis.bestLapMs());
    st.sessionBestLiveDeltaSeconds = dash.liveDeltaText();
    st.tyrePressureRearLeft = String(driver.lapCount) + "/10";
    st.tyrePressureFrontRight = "3";
    st.tyrePressureFrontLeft = "6.2";
    st.tcLevel = String(static_cast<int>(lroundf(now.throttle * 100)));
    st.absLevel = String(static_cast<int>(lroundf(now.brake * 100)));
    st.fuelProgressPercent = static_cast<int>(driver.fuel * 100);
    st.fuelDisplayValue = String(st.fuelProgressPercent);
    st.brakeBias = st.fuelDisplayValue;
    st.tyreTemperatures[0] = 88; st.tyreTemperatures[1] = 93;
    st.tyreTemperatures[2] = 81; st.tyreTemperatures[3] = 84;
    st.gameRunning = "True";
    return st;
}

static DashboardState limiterState(DashboardState st)
{
    st.gear = "6";
    st.rpmPercent = 100;
    st.engineRpm = 8000;
    st.revLimitAlertActive = true;
    st.speed = "247";
    st.suggestedGear = "";
    return st;
}

static void renderTheme(DashboardTheme theme, const DashboardState &state)
{
    dash.activeDashboardTheme = theme;
    dash.invalidateDashboardRenderer();
    tft.fillScreen(TFT_BLACK);
    // Two passes: the first builds the static layout, the second the values.
    DashboardState st = state;
    dash.renderDashboard(theme, st, true);
    // Second pass in the "on" phase of the 70 ms limiter flash.
    g_hostMillis = (g_hostMillis / 140 + 1) * 140;
    dash.renderDashboard(theme, st, false);
}

static std::string slug(const char *name)
{
    std::string out;
    for (const char *p = name; *p; ++p)
        out += *p == ' ' ? '-' : (char)tolower((unsigned char)*p);
    return out;
}

static void settingsScreen(SHCustomProtocol::SettingsScreen screen, const char *name,
                           std::function<void()> before = nullptr)
{
    dash.settingsScreen = screen;
    if (before) before();
    tft.fillScreen(TFT_BLACK);
    dash.drawSettingsScreen();
    savePng(name);
}

int main(int argc, char **argv)
{
    if (argc > 1) g_outDir = argv[1];
    tft.init();
    dash.prepareThemePreviewData();
    dash.loadDashboardTheme();
    dash.wifiConfigured = true;
    dash.telemetry.mode = TelemetryMode::GT7;
    buildTrack();
    printf("Circuito simulato: %.0f m\n", track.length);

    // Out-lap from the second half of the circuit, then lap 1 records the map.
    const float steady[3] = {1.0f, 1.0f, 1.0f};
    driver.s = track.length * 0.6f;
    driver.speed = track.speedAt(driver.s);
    driver.drive(steady, [] { return driver.lapCount == 1 && lapFraction() > 0.46f; });

    printf("Mappa in costruzione:\n");
    renderTheme(DashboardTheme::TrackMap, simState());
    savePng("theme-mappa-pista-costruzione");

    driver.drive(steady, [] { return driver.lapCount == 2; });
    const float faster[3] = {1.012f, 1.008f, 1.010f};
    driver.drive(faster, [] { return driver.lapCount == 3; });
    const float mixed[3] = {1.016f, 0.992f, 1.004f};
    driver.drive(mixed, [] { return driver.lapCount == 4; });
    // Lap 4: capture in the middle of a corner of the second sector.
    const float attack[3] = {1.018f, 1.011f, 1.0f};
    driver.drive(attack, [] {
        return driver.lapCount == 4 && lapFraction() > 0.52f && fabsf(dash.lapAnalysis.lateralG()) > 1.15f;
    });
    const DashboardState race = simState();
    printf("Giro %d al %.0f%%: %s km/h, delta %s, G lat %.2f\n", driver.lapCount, lapFraction() * 100,
           race.speed.c_str(), race.sessionBestLiveDeltaSeconds.c_str(), dash.lapAnalysis.lateralG());

    printf("Temi della dashboard:\n");
    for (const auto &theme : DASHBOARD_THEMES)
    {
        if (theme.id == DashboardTheme::Performance) continue;
        const std::string base = "theme-" + slug(theme.name);
        renderTheme(theme.id, race);
        savePng(base);
        renderTheme(theme.id, limiterState(race));
        savePng(base + "-limitatore");
    }

    // Acceleration run: stop, wait for the green light, full throttle to 250.
    printf("Prestazioni:\n");
    const float stop[3] = {0, 0, 0};
    driver.drive(stop, [] { return driver.speed <= 0.0f; });
    driver.drive(stop, [] { return dash.lapAnalysis.treeLight() == LapAnalysis::TreeLight::Amber2; });
    renderTheme(DashboardTheme::Performance, simState());
    savePng("theme-prestazioni-partenza");
    driver.drive(stop, [] { return dash.lapAnalysis.treeLight() == LapAnalysis::TreeLight::Green; });
    for (int i = 0; i < 14; ++i) driver.step(0); // reaction time
    while (driver.speed < 250 / 3.6f) driver.step(90.0f);
    for (int i = 0; i < 20; ++i) driver.step(0); // braking ends the run
    renderTheme(DashboardTheme::Performance, simState());
    savePng("theme-prestazioni");
    renderTheme(DashboardTheme::Performance, limiterState(simState()));
    savePng("theme-prestazioni-limitatore");

    printf("Notifiche:\n");
    const auto notification = [&race](LapAnalysis::EventType type, int32_t lapMs, int32_t gainMs,
                                       const char *detail, const char *name) {
        dash.shownNotification = SHCustomProtocol::DashboardNotification();
        dash.shownNotification.type = type;
        dash.shownNotification.lapMs = lapMs;
        dash.shownNotification.gainMs = gainMs;
        dash.shownNotification.detail = detail;
        dash.drawNotification(race, true);
        savePng(name);
    };
    notification(LapAnalysis::EventType::BestLap, dash.lapAnalysis.bestLapMs(), 214, "", "notifica-giro-migliore");
    notification(LapAnalysis::EventType::FinalLap, -1, -1, "GIRO 10/10", "notifica-ultimo-giro");
    notification(LapAnalysis::EventType::LowFuel, -1, -1, "1.4 GIRI", "notifica-riserva");

    printf("Schermata di attesa:\n");
    static const char *const backgrounds[3] = {"waiting-minimale", "waiting-racing", "waiting-riepilogo"};
    for (uint8_t bg = 0; bg < 3; ++bg)
    {
        dash.waitingBackground = bg;
        dash.ledStripFound = bg != 0;
        dash.connectingScreenActive = false;
        tft.fillScreen(TFT_BLACK);
        dash.drawConnectingScreenBase();
        savePng(backgrounds[bg]);
    }

    printf("Menu e impostazioni:\n");
    using S = SHCustomProtocol::SettingsScreen;
    settingsScreen(S::Main, "menu-impostazioni");
    settingsScreen(S::ThemeSelection, "menu-selezione-tema", [] {
        dash.previewDashboardTheme = DashboardTheme::Formula;
        dash.previewFullscreen = false;
        dash.clearThemePreviewRenderCache();
    });
    settingsScreen(S::BackgroundSelection, "menu-sfondo-attesa", [] { dash.previewWaitingBackground = 2; });
    settingsScreen(S::Features, "menu-funzioni", [] { dash.featuresStatus = ""; });
    settingsScreen(S::DeviceSettings, "menu-dispositivo");
    settingsScreen(S::LedSettings, "menu-led", [] {
        dash.ledStripFound = true;
        dash.ledTheme = 1;
        dash.ledIdleMode = 2;
        dash.ledBrightness = 180;
    });
    settingsScreen(S::LedSettings, "menu-led-non-trovata", [] { dash.ledStripFound = false; });
    settingsScreen(S::ConnectionChoice, "menu-scelta-connessione", [] { dash.connectionChoiceCanCancel = true; });
    settingsScreen(S::WifiSettings, "menu-wifi");
    settingsScreen(S::ResetConfirmation, "menu-ripristino");
    settingsScreen(S::InitialTouch, "menu-calibrazione-touch");
    return 0;
}
