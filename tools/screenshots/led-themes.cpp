// Previews of the LED strip behind the TV for the README: the real effects
// (firmware/led-strip/src/LedEffects.h) fed by the simulated race of the
// tests (firmware/led-strip/tests/sim_race.h), drawn as a glow around a TV.
//   g++ -std=c++11 -O2 tools/screenshots/led-themes.cpp -lz -o /tmp/led-themes && /tmp/led-themes docs/schermate
#include "../../firmware/led-strip/tests/sim_race.h"
#include <stdio.h>
#include <string>
#include <vector>
#include <zlib.h>

using namespace LedFx;

static const int W = 480, H = 300;
static std::string outDir = "docs/schermate";

static void chunk(FILE *f, const char *type, const std::vector<unsigned char> &data)
{
    unsigned char len[4] = {(unsigned char)(data.size() >> 24), (unsigned char)(data.size() >> 16),
                            (unsigned char)(data.size() >> 8), (unsigned char)data.size()};
    fwrite(len, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (!data.empty()) fwrite(data.data(), 1, data.size(), f);
    uLong crc = crc32(0, (const Bytef *)type, 4);
    if (!data.empty()) crc = crc32(crc, data.data(), data.size());
    unsigned char c[4] = {(unsigned char)(crc >> 24), (unsigned char)(crc >> 16), (unsigned char)(crc >> 8), (unsigned char)crc};
    fwrite(c, 1, 4, f);
}

static void savePng(const std::string &name, const std::vector<float> &rgb)
{
    std::vector<unsigned char> raw;
    for (int y = 0; y < H; ++y)
    {
        raw.push_back(0);
        for (int x = 0; x < W * 3; ++x)
        {
            const float v = rgb[y * W * 3 + x];
            raw.push_back((unsigned char)(v <= 0 ? 0 : v >= 255 ? 255 : v + 0.5f));
        }
    }
    uLongf size = compressBound(raw.size());
    std::vector<unsigned char> z(size);
    compress2(z.data(), &size, raw.data(), raw.size(), 9);
    z.resize(size);
    const std::string path = outDir + "/" + name + ".png";
    FILE *f = fopen(path.c_str(), "wb");
    if (!f) { perror(path.c_str()); return; }
    const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    std::vector<unsigned char> ihdr = {0, 0, (unsigned char)(W >> 8), (unsigned char)W, 0, 0, (unsigned char)(H >> 8), (unsigned char)H, 8, 2, 0, 0, 0};
    chunk(f, "IHDR", ihdr);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    fclose(f);
    printf("  %s\n", path.c_str());
}

// The TV seen from the front, the strip glowing on the wall behind it.
static void draw(const Renderer &r, const Rgb *frame, const std::string &name)
{
    std::vector<float> img(W * H * 3);
    for (int i = 0; i < W * H; ++i) { img[i * 3] = 14; img[i * 3 + 1] = 15; img[i * 3 + 2] = 19; }
    const float X0 = 62, X1 = 418, Y0 = 32, Y1 = 240; // the strip, just inside the edges of the TV
    const Layout &l = r.layout();
    const int n = r.count();
    const int first = l.startLeft ? l.left : l.right, last = l.startLeft ? l.right : l.left;
    std::vector<float> px(n), py(n);
    for (int i = 0; i < n; ++i)
    {
        if (i < first) { const float k = (i + 0.5f) / first; px[i] = l.startLeft ? X0 : X1; py[i] = Y1 - k * (Y1 - Y0); }
        else if (i < first + l.top) { const float k = (i - first + 0.5f) / l.top; px[i] = l.startLeft ? X0 + k * (X1 - X0) : X1 - k * (X1 - X0); py[i] = Y0; }
        else { const float k = (i - first - l.top + 0.5f) / last; px[i] = l.startLeft ? X1 : X0; py[i] = Y0 + k * (Y1 - Y0); }
    }
    // Glow on the wall: each LED lights a wide soft spot, pushed outwards.
    for (int i = 0; i < n; ++i)
    {
        const Rgb c = frame[i];
        if (!c.r && !c.g && !c.b) continue;
        float ox = 0, oy = 0;
        if (py[i] == Y0) oy = -22;
        else ox = px[i] == X1 ? 22 : -22;
        const float cx = px[i] + ox, cy = py[i] + oy, rad = 34;
        for (int y = (int)(cy - rad); y <= (int)(cy + rad); ++y)
            for (int x = (int)(cx - rad); x <= (int)(cx + rad); ++x)
            {
                if (x < 0 || y < 0 || x >= W || y >= H) continue;
                const float d2 = ((x - cx) * (x - cx) + (y - cy) * (y - cy)) / (rad * rad);
                if (d2 >= 1) continue;
                const float k = (1 - d2) * (1 - d2) * 0.16f;
                float *p = &img[(y * W + x) * 3];
                p[0] += c.r * k; p[1] += c.g * k; p[2] += c.b * k;
            }
    }
    // The TV: black screen with a thin bezel, on a stand.
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
        {
            float *p = &img[(y * W + x) * 3];
            const bool tv = x >= 52 && x < 428 && y >= 22 && y < 250;
            const bool stand = (x >= 225 && x < 255 && y >= 250 && y < 268) || (x >= 180 && x < 300 && y >= 268 && y < 274);
            if (tv) { const bool bezel = x < 56 || x >= 424 || y < 26 || y >= 246; p[0] = bezel ? 30 : 6; p[1] = bezel ? 32 : 7; p[2] = bezel ? 38 : 10; }
            else if (stand) { p[0] = 30; p[1] = 32; p[2] = 38; }
        }
    savePng(name, img);
}

static SimRace sim;
static Race::Telemetry race;

static void driveTo(float fraction, int lap)
{
    while (!(sim.lapCount == lap && sim.lapFraction() >= fraction))
    {
        const Race::Packet p = sim.step();
        race.update(p, sim.nowMs);
        LedFx::Event e;
        while (race.takeEvent(e)) {}
        sim.pace = sim.lapCount == 2 ? 0.97f : 1.0f;
    }
}

// Data for the screenshot of the web page (tools/screenshots/led-web.mjs):
// frames at 20 Hz from the start of lap 2 to the middle of lap 3, the
// session at the end and the colours of the strip.
static void webData(const char *path)
{
    SimRace s2;
    Race::Telemetry t2;
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_DELTA;
    static Rgb frame[MAX_LEDS];
    std::string out = "{\"frames\":[";
    static char buf[2048];
    bool first = true;
    for (int k = 0; s2.lapCount < 3 || s2.lapFraction() < 0.62f; ++k)
    {
        s2.pace = s2.lapCount == 2 ? 0.985f : 1.0f;
        t2.update(s2.step(), s2.nowMs);
        LedFx::Event e;
        while (t2.takeEvent(e)) {}
        if (s2.lapCount < 2 || k % 3) continue;
        JsonOut j(buf, sizeof(buf));
        t2.writeFrame(j, s2.nowMs);
        if (!first) out += ",";
        out += buf;
        first = false;
    }
    std::vector<char> big(16384);
    JsonOut js(big.data(), big.size());
    t2.writeSession(js);
    LedFx::Input in;
    t2.fillInput(in, s2.nowMs);
    for (int k = 0; k < 30; ++k) r.render(in, s2.nowMs + k * 16, frame);
    std::string hex;
    for (int i = 0; i < r.count(); ++i)
    {
        char h[8];
        snprintf(h, sizeof(h), "%02x%02x%02x", frame[i].r, frame[i].g, frame[i].b);
        hex += h;
    }
    out += "],\"session\":" + std::string(big.data()) + ",\"leds\":\"" + hex + "\"}";
    FILE *f = fopen(path, "w");
    if (!f) { perror(path); return; }
    fputs(out.c_str(), f);
    fclose(f);
    printf("  %s\n", path);
}

int main(int argc, char **argv)
{
    if (argc > 1) outDir = argv[1];
    if (argc > 2) webData(argv[2]);
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.gurgle = 0;
    r.settings.alerts = 0;
    static Rgb frame[MAX_LEDS];
    static const char *themeSlugs[] = {"default", "f1-center", "supercar", "smooth-fade", "f1-reverse", "default-rev",
                                       "delta", "pedali", "velocita", "forze-g", "gomme", "ambiente", "solo-avvisi"};
    // Lap 3, accelerating out of a corner, ahead of the best lap.
    driveTo(0.30f, 3);
    printf("Temi in gara:\n");
    for (int t = 0; t < THEME_COUNT; ++t)
    {
        r.settings.theme = (uint8_t)t;
        LedFx::Input in;
        race.fillInput(in, sim.nowMs);
        in.rpm = 6900;
        in.throttle = 0.85f;
        in.brake = 0;
        if (t == THEME_DELTA) { in.hasDelta = true; in.deltaS = -0.42f; }
        if (t == THEME_G_FORCE) { in.lateralG = 1.25f; in.longitudinalG = -0.5f; }
        if (t == THEME_PEDALS) { in.throttle = 0.55f; in.brake = 0.8f; }
        if (t == THEME_TYRES) { in.tyreTemp[0] = 104; in.tyreTemp[1] = 92; in.tyreTemp[2] = 70; in.tyreTemp[3] = 58; }
        if (t == THEME_AMBIENT) in.timeOfDayH = 19.2f;
        if (t == THEME_ALERTS_ONLY) r.settings.idleColor = Rgb(255, 140, 40);
        in.speedKmh = 240;
        uint32_t now = sim.nowMs;
        for (int k = 0; k < 90; ++k, now += 16) r.render(in, now, frame); // let the animations settle
        draw(r, frame, std::string("led-tema-") + themeSlugs[t]);
    }
    // Delta behind.
    {
        r.settings.theme = THEME_DELTA;
        LedFx::Input in;
        race.fillInput(in, sim.nowMs);
        in.rpm = 5200; in.hasDelta = true; in.deltaS = 0.7f;
        uint32_t now = sim.nowMs + 5000;
        for (int k = 0; k < 40; ++k, now += 16) r.render(in, now, frame);
        draw(r, frame, "led-tema-delta-ritardo");
    }
    printf("Avvisi:\n");
    r.settings.theme = THEME_DEFAULT;
    r.settings.alerts = ALERTS_ALL;
    struct Shot { const char *name; Event e; int afterMs; };
    const Shot shots[] = {{"led-avviso-giro-migliore", Event::BestLap, 200}, {"led-avviso-settore", Event::SectorPurple, 300},
                          {"led-avviso-arrivo", Event::Finish, 300}, {"led-avviso-riserva", Event::LowFuel, 200}};
    uint32_t now = sim.nowMs + 20000;
    for (const Shot &s : shots)
    {
        LedFx::Input in;
        race.fillInput(in, now);
        in.racing = true;
        in.rpm = 4500;
        in.limiter = false;
        in.hasBrakePoint = false;
        r.trigger(s.e, now);
        for (int k = 0; k <= s.afterMs; k += 16) r.render(in, now + k, frame);
        draw(r, frame, s.name);
        now += 10000;
    }
    {
        LedFx::Input in;
        race.fillInput(in, now);
        in.racing = true;
        in.rpm = 5600; in.speedKmh = 230; in.brake = 0; in.hasBrakePoint = true; in.brakePointM = 70;
        r.render(in, now, frame);
        draw(r, frame, "led-avviso-frenata");
        in.hasBrakePoint = false; in.wheel[0] = WHEEL_LOCKING;
        r.render(in, now + 16, frame);
        draw(r, frame, "led-avviso-bloccaggio");
        in.wheel[0] = WHEEL_GRIP; in.rpm = 8000; in.limiter = true;
        r.render(in, now + 100, frame);
        draw(r, frame, "led-avviso-limitatore");
        r.startLayoutTest(now);
        r.render(in, now + 300, frame);
        draw(r, frame, "led-prova-montaggio");
        r.stopLayoutTest();
    }
    printf("Gioco fermo:\n");
    static const char *idleSlugs[] = {"spento", "colore-fisso", "respiro", "arcobaleno", "onda", "fuoco", "aurora", "luce-tv"};
    r.settings.idleColor = Rgb(255, 200, 0);
    for (int m = 1; m < IDLE_COUNT; ++m)
    {
        r.settings.idleMode = (uint8_t)m;
        LedFx::Input in; // not racing
        uint32_t t = now + 20000 + m * 7000;
        for (int k = 0; k < 150; ++k, t += 16) r.render(in, t, frame);
        if (m == IDLE_WAVE) { t = (t / 2600 + 1) * 2600 + 1200; r.render(in, t, frame); }
        draw(r, frame, std::string("led-riposo-") + idleSlugs[m]);
    }
    return 0;
}
