// Renders every dashboard screen of firmware/dashboard to 320x240 PNG files,
// running the real drawing code (themes, menus, waiting screen) on a PC.
// Build and run with tools/screenshots/build.sh.
#include <Arduino.h>
#include <WiFi.h>
#include <zlib.h>
#include <functional>
#include <string>
#include <vector>

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

// A lap in progress: values similar to a real GT7 session.
static DashboardState raceState(const char *gear, int rpmPercent)
{
    DashboardState s = dash.themePreviewData;
    s.gear = gear;
    s.rpmPercent = rpmPercent;
    s.engineRpm = 7400 * rpmPercent / 100;
    s.rpmRedLineSetting = 90;
    s.rpmAlertRangeValid = true;
    s.revLimitAlertActive = rpmPercent >= 98;
    s.gameRunning = "True";
    s.suggestedGear = rpmPercent < 90 ? "5" : ""; // marcia consigliata (Ferrari Gold, BMW)
    return s;
}

static void renderTheme(DashboardTheme theme, DashboardState state)
{
    dash.activeDashboardTheme = theme;
    dash.invalidateDashboardRenderer();
    tft.fillScreen(TFT_BLACK);
    // Two passes: the first builds the static layout, the second the values.
    dash.renderDashboard(theme, state, true);
    g_hostMillis += 40;
    dash.renderDashboard(theme, state, false);
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

    printf("Temi della dashboard:\n");
    for (const auto &theme : DASHBOARD_THEMES)
    {
        const std::string base = "theme-" + slug(theme.name);
        renderTheme(theme.id, raceState("4", 72));
        // -v2: nome nuovo dopo lo spostamento della marcia consigliata, così i
        // browser non mostrano l'immagine vecchia rimasta in cache
        const bool renamed = theme.id == DashboardTheme::FerrariGold || theme.id == DashboardTheme::Bmw;
        savePng(renamed ? base + "-v2" : base);
        renderTheme(theme.id, raceState("6", 99));
        savePng(base + "-limitatore");
    }

    printf("Schermata di attesa:\n");
    for (uint8_t bg = 0; bg < 2; ++bg)
    {
        dash.waitingBackground = bg;
        dash.ledStripFound = bg == 1;
        dash.connectingScreenActive = false;
        tft.fillScreen(TFT_BLACK);
        dash.drawConnectingScreenBase();
        savePng(bg == 0 ? "waiting-minimale" : "waiting-racing");
    }

    printf("Menu e impostazioni:\n");
    using S = SHCustomProtocol::SettingsScreen;
    settingsScreen(S::Main, "menu-impostazioni");
    settingsScreen(S::ThemeSelection, "menu-selezione-tema", [] {
        dash.previewDashboardTheme = DashboardTheme::Ferrari;
        dash.previewFullscreen = false;
        dash.clearThemePreviewRenderCache();
    });
    settingsScreen(S::BackgroundSelection, "menu-sfondo-attesa", [] { dash.previewWaitingBackground = 1; });
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
