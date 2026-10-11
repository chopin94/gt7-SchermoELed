#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>

// Light effects of the LED strip behind the TV. Plain C++11 without Arduino
// types, so the same code runs on the ESP32-S2, in the unit tests
// (tests/led_effects.cpp) and in the preview renderer
// (tools/screenshots/led-themes.cpp).
//
// The strip frames the TV on three sides: it starts at the bottom of the right
// side, goes up, along the top from right to left and down the left side.
// There are no LEDs along the bottom edge. Layout::startLeft mirrors it; with
// both sides at 0 the strip is a plain bar and the effects use its outer
// quarters as the two sides.
//
// The ESP32-S2 has no floating point unit: the loops over the LEDs use plain
// arithmetic only, sines and colours come from the integer code of FastLED
// (ported here, so the first six themes look exactly as before).
namespace LedFx
{
static constexpr int MAX_LEDS = 200;

struct Rgb
{
    uint8_t r, g, b;
    constexpr Rgb() : r(0), g(0), b(0) {}
    constexpr Rgb(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
    bool operator==(const Rgb &o) const { return r == o.r && g == o.g && b == o.b; }
    bool operator!=(const Rgb &o) const { return !(*this == o); }
};

// The colours of the original themes are FastLED's named colours:
// CRGB::Green is the dark HTML green.
static constexpr Rgb BLACK(0, 0, 0), WHITE(255, 255, 255);
static constexpr Rgb RED(255, 0, 0), GREEN(0, 128, 0), YELLOW(255, 255, 0), BLUE(0, 0, 255);
static constexpr Rgb SHIFT_GREEN(0, 200, 0), SHIFT_YELLOW(255, 170, 0), SHIFT_RED(255, 0, 0);
static constexpr Rgb AHEAD(0, 230, 40), BEHIND(255, 0, 0);
static constexpr Rgb PURPLE(170, 0, 255), AMBER(255, 110, 0), ORANGE(255, 60, 0), CYAN(0, 190, 255);

// Indices shared with the screen (SHCustomProtocol.h) and the web page.
enum Theme : uint8_t
{
    THEME_DEFAULT = 0,
    THEME_F1_CENTER = 1,
    THEME_SUPERCAR = 2,
    THEME_SMOOTH_FADE = 3,
    THEME_F1_REVERSE = 4,
    THEME_DEFAULT_REVERSE = 5,
    THEME_DELTA = 6,       // sides: live delta, top: revs
    THEME_PEDALS = 7,      // right: throttle, left: brake, top: revs
    THEME_SPEED = 8,       // light streaks running out from the centre
    THEME_G_FORCE = 9,     // a light that moves like the load on the body
    THEME_TYRES = 10,      // temperature of each tyre in its corner
    THEME_AMBIENT = 11,    // the colour of the sky at the time of day of the race
    THEME_ALERTS_ONLY = 12, // a still light, only the alerts move
    THEME_COUNT = 13
};

enum IdleMode : uint8_t
{
    IDLE_OFF = 0,
    IDLE_SOLID = 1,
    IDLE_BREATHING = 2,
    IDLE_RAINBOW = 3,
    IDLE_WAVE = 4,   // two comets climbing to the top centre
    IDLE_FIRE = 5,   // flames up both sides
    IDLE_AURORA = 6, // slow northern lights
    IDLE_TV = 7,     // neutral white bias light for watching TV
    IDLE_COUNT = 8
};

// Alerts drawn over any theme, each one can be switched off.
enum Alert : uint16_t
{
    ALERT_LIMITER = 1 << 0,  // red flashes at the rev limiter (GT7 flag)
    ALERT_BEST_LAP = 1 << 1, // purple pulses for a new best lap
    ALERT_SECTORS = 1 << 2,  // the top takes the colour of each sector just closed
    ALERT_BRAKING = 1 << 3,  // countdown to the braking point of the best lap
    ALERT_FUEL = 1 << 4,     // amber pulses when the fuel runs low
    ALERT_GRIP = 1 << 5,     // lock-ups (red) and wheelspin (orange) on the side of the wheel
    ALERT_FLAGS = 1 << 6,    // white flashes on the final lap, chequered flag at the finish
    ALERT_GEAR = 1 << 7,     // cyan corners while the suggested gear is lower
};
static constexpr uint16_t ALERTS_ALL = 0xFF;
static constexpr uint16_t ALERTS_DEFAULT = ALERT_LIMITER | ALERT_BEST_LAP | ALERT_SECTORS | ALERT_FUEL | ALERT_FLAGS;

enum class Zone : uint8_t { Right, Top, Left };

enum class Event : uint8_t
{
    BestLap,
    SectorPurple, // sector closed with the best time of the session
    SectorGreen,  // faster than the same sector of the best lap
    SectorYellow, // slower
    LowFuel,
    FinalLap,
    Finish,
};

// How the strip is mounted: LEDs on each side of the TV.
struct Layout
{
    int right = 38, top = 67, left = 38;
    bool startLeft = false; // the strip starts at the bottom of the left side

    int count() const { return right + top + left; }
    bool valid() const { return right >= 0 && top >= 0 && left >= 0 && count() >= 1 && count() <= MAX_LEDS; }

    // A 16:9 TV: each side is 9/16 of the top.
    static Layout forCount(int count)
    {
        if (count < 1) count = 1;
        if (count > MAX_LEDS) count = MAX_LEDS;
        int side = static_cast<int>(count / (2.0f + 16.0f / 9.0f) + 0.5f);
        if (2 * side > count) side = count / 2;
        Layout l;
        l.right = l.left = side;
        l.top = count - 2 * side;
        return l;
    }

    // The same proportions with another number of LEDs.
    Layout resized(int count) const
    {
        if (count < 1) count = 1;
        if (count > MAX_LEDS) count = MAX_LEDS;
        const int total = this->count();
        Layout l = total > 0 ? *this : forCount(count);
        l.startLeft = startLeft;
        if (total > 0 && count != total)
        {
            l.right = static_cast<int>(static_cast<float>(right) * count / total + 0.5f);
            l.left = static_cast<int>(static_cast<float>(left) * count / total + 0.5f);
            if (l.right + l.left > count) l.left = count - l.right;
            l.top = count - l.right - l.left;
        }
        return l;
    }
};

// What the effects know about the car at this moment.
struct Input
{
    bool racing = false;      // on track and not paused: the race theme, otherwise the idle animation
    float rpm = 0;
    float maxRpm = 0;         // top of the scale (GT7 maxAlertRPM)
    float alertRpm = 0;       // where the game's shift light starts (GT7 minAlertRPM)
    bool limiter = false;     // GT7 rev limiter flag
    float speedKmh = 0;
    float throttle = 0;       // 0-1
    float brake = 0;          // 0-1
    int gear = 0;
    int suggestedGear = -1;   // -1 when the game suggests none
    bool hasDelta = false;
    float deltaS = 0;         // live delta in seconds, + behind the best lap
    float lateralG = 0;       // + in a right turn
    float longitudinalG = 0;  // + accelerating
    float tyreTemp[4] = {NAN, NAN, NAN, NAN}; // front left, front right, rear left, rear right
    uint8_t wheel[4] = {0, 0, 0, 0};          // WHEEL_* of each wheel
    float timeOfDayH = NAN;   // in-game hour 0-24
    bool hasBrakePoint = false;
    float brakePointM = 0;    // metres to the next braking point of the best lap
};
enum : uint8_t { WHEEL_UNKNOWN = 0, WHEEL_GRIP = 1, WHEEL_LOCKING = 2, WHEEL_SPINNING = 3 }; // GripAnalysis::WheelState

struct Settings
{
    uint8_t theme = THEME_DEFAULT;
    uint8_t idleMode = IDLE_BREATHING;
    Rgb idleColor = Rgb(255, 255, 0);
    uint8_t gurgle = 150;           // flicker at the tip of the bars, 0 = off
    uint16_t alerts = ALERTS_DEFAULT;
    float deltaScaleS = 1.0f;       // delta that fills the sides
};

// ---- FastLED's integer colour code (lib8tion, hsv2rgb_rainbow, HeatColor) ----
inline uint8_t scale8(uint8_t i, uint8_t scale)
{
    return static_cast<uint8_t>((static_cast<uint16_t>(i) * (1 + static_cast<uint16_t>(scale))) >> 8);
}
inline uint8_t scale8Video(uint8_t i, uint8_t scale)
{
    return static_cast<uint8_t>(((static_cast<int>(i) * static_cast<int>(scale)) >> 8) + ((i && scale) ? 1 : 0));
}
inline uint8_t qadd8(uint8_t a, uint8_t b) { const int s = a + b; return static_cast<uint8_t>(s > 255 ? 255 : s); }
inline uint8_t qsub8(uint8_t a, uint8_t b) { return static_cast<uint8_t>(a > b ? a - b : 0); }
inline Rgb scaled(Rgb c, uint8_t s) { return Rgb(scale8(c.r, s), scale8(c.g, s), scale8(c.b, s)); }
inline Rgb added(Rgb a, Rgb b) { return Rgb(qadd8(a.r, b.r), qadd8(a.g, b.g), qadd8(a.b, b.b)); }
// amount 0 gives a, 255 gives b.
inline Rgb blended(Rgb a, Rgb b, uint8_t amount)
{
    const int k = amount, j = 255 - amount;
    return Rgb(static_cast<uint8_t>((a.r * j + b.r * k + 127) / 255), static_cast<uint8_t>((a.g * j + b.g * k + 127) / 255),
               static_cast<uint8_t>((a.b * j + b.b * k + 127) / 255));
}
inline uint8_t unit8(float v) { return v <= 0.0f ? 0 : v >= 1.0f ? 255 : static_cast<uint8_t>(v * 255.0f + 0.5f); }

inline uint8_t sin8(uint8_t theta)
{
    static const uint8_t table[] = {0, 49, 49, 41, 90, 27, 117, 10};
    uint8_t offset = theta;
    if (theta & 0x40) offset = static_cast<uint8_t>(255 - offset);
    offset &= 0x3F;
    uint8_t secoffset = offset & 0x0F;
    if (theta & 0x40) ++secoffset;
    const uint8_t section = offset >> 4;
    const uint8_t b = table[section * 2], m16 = table[section * 2 + 1];
    const uint8_t mx = static_cast<uint8_t>((m16 * secoffset) >> 4);
    int8_t y = static_cast<int8_t>(mx + b);
    if (theta & 0x80) y = static_cast<int8_t>(-y);
    return static_cast<uint8_t>(y + 128);
}

inline Rgb hsv(uint8_t hue, uint8_t sat, uint8_t val)
{
    const uint8_t offset8 = static_cast<uint8_t>((hue & 0x1F) << 3);
    const uint8_t third = scale8(offset8, 256 / 3);
    const uint8_t twothirds = scale8(offset8, (256 * 2) / 3);
    uint8_t r, g, b;
    switch (hue >> 5)
    {
    case 0: r = 255 - third; g = third; b = 0; break;          // red to orange
    case 1: r = 171; g = 85 + third; b = 0; break;             // orange to yellow
    case 2: r = 171 - twothirds; g = 170 + third; b = 0; break; // yellow to green
    case 3: r = 0; g = 255 - third; b = third; break;          // green to aqua
    case 4: r = 0; g = 171 - twothirds; b = 85 + twothirds; break; // aqua to blue
    case 5: r = third; g = 0; b = 255 - third; break;          // blue to purple
    case 6: r = 85 + third; g = 0; b = 171 - third; break;     // purple to pink
    default: r = 170 + third; g = 0; b = 85 - third; break;    // pink to red
    }
    if (sat != 255)
    {
        if (sat == 0) r = g = b = 255;
        else
        {
            const uint8_t desat = scale8Video(255 - sat, 255 - sat);
            const uint8_t satscale = 255 - desat;
            r = static_cast<uint8_t>(scale8(r, satscale) + desat);
            g = static_cast<uint8_t>(scale8(g, satscale) + desat);
            b = static_cast<uint8_t>(scale8(b, satscale) + desat);
        }
    }
    if (val != 255)
    {
        val = scale8Video(val, val);
        if (val == 0) r = g = b = 0;
        else { r = scale8(r, val); g = scale8(g, val); b = scale8(b, val); }
    }
    return Rgb(r, g, b);
}

inline Rgb heatColor(uint8_t temperature)
{
    const uint8_t t192 = scale8Video(temperature, 191);
    const uint8_t ramp = static_cast<uint8_t>((t192 & 0x3F) << 2);
    if (t192 & 0x80) return Rgb(255, 255, ramp);
    if (t192 & 0x40) return Rgb(255, ramp, 0);
    return Rgb(ramp, 0, 0);
}

// The tyre colours of the screen's analysis themes, blended between the steps.
inline Rgb tyreColor(float celsius)
{
    if (!(celsius > 0.0f)) return Rgb(18, 18, 22); // NAN or no data
    static const float steps[] = {50, 62, 72, 82, 95, 102, 110};
    static const Rgb colors[] = {Rgb(30, 60, 255), Rgb(40, 110, 230), Rgb(40, 180, 210), Rgb(0, 190, 80),
                                 Rgb(0, 190, 80), Rgb(255, 170, 0), Rgb(226, 30, 36)};
    if (celsius <= steps[0]) return colors[0];
    for (int i = 1; i < 7; ++i)
        if (celsius < steps[i])
            return blended(colors[i - 1], colors[i], unit8((celsius - steps[i - 1]) / (steps[i] - steps[i - 1])));
    return colors[6];
}

// The sky at an hour of the day (0-24): night, dawn, day, sunset.
inline Rgb skyColor(float hour)
{
    static const float steps[] = {0, 4.5f, 6, 7, 9, 12, 16, 18, 19.5f, 20.5f, 22, 24};
    static const Rgb colors[] = {Rgb(4, 8, 60), Rgb(4, 8, 60), Rgb(90, 30, 110), Rgb(255, 90, 30),
                                 Rgb(255, 190, 120), Rgb(150, 200, 255), Rgb(170, 205, 255), Rgb(255, 170, 80),
                                 Rgb(255, 60, 15), Rgb(110, 20, 110), Rgb(10, 12, 70), Rgb(4, 8, 60)};
    if (!(hour >= 0.0f)) return Rgb(4, 8, 60);
    hour = fmodf(hour, 24.0f);
    for (int i = 1; i < 12; ++i)
        if (hour < steps[i])
            return blended(colors[i - 1], colors[i], unit8((hour - steps[i - 1]) / (steps[i] - steps[i - 1])));
    return colors[11];
}

class Renderer
{
public:
    Settings settings;

    Renderer() { setLayout(Layout()); }

    void setLayout(const Layout &layout)
    {
        layout_ = layout.valid() ? layout : Layout::forCount(layout.count());
        computeGeometry();
        for (int h = 0; h < 2; ++h)
        {
            for (int k = 0; k < MAX_PARTICLES; ++k) particles[h][k].alive = false;
            memset(heat[h], 0, sizeof(heat[h]));
        }
    }
    const Layout &layout() const { return layout_; }
    int count() const { return n; }

    // Starts the animation of an event (it is drawn only if its alert is on).
    void trigger(Event event, uint32_t nowMs)
    {
        switch (event)
        {
        case Event::BestLap: bestLapMs = nowMs | 1; break;
        case Event::SectorPurple: sectorMs = nowMs | 1; sectorColor = PURPLE; break;
        case Event::SectorGreen: sectorMs = nowMs | 1; sectorColor = AHEAD; break;
        case Event::SectorYellow: sectorMs = nowMs | 1; sectorColor = SHIFT_YELLOW; break;
        case Event::LowFuel: fuelMs = nowMs | 1; break;
        case Event::FinalLap: finalLapMs = nowMs | 1; break;
        case Event::Finish: finishMs = nowMs | 1; break;
        }
    }

    // Each side in its own colour, to check the layout: right red, top
    // green, left blue, the first LED blinking white.
    void startLayoutTest(uint32_t nowMs, uint32_t durationMs = 15000)
    {
        testUntilMs = nowMs + durationMs;
        testActive = true;
    }
    void stopLayoutTest() { testActive = false; }
    bool layoutTestActive(uint32_t nowMs) const { return testActive && static_cast<int32_t>(testUntilMs - nowMs) > 0; }

    // One frame of count() colours.
    void render(const Input &in, uint32_t nowMs, Rgb *out)
    {
        const uint32_t dtMs = started ? (nowMs - lastFrameMs > 100 ? 100 : nowMs - lastFrameMs) : 0;
        started = true;
        lastFrameMs = nowMs;
        frame = out;
        for (int i = 0; i < n; ++i) out[i] = BLACK;
        updateFlicker(nowMs);

        const bool racing = in.racing && in.maxRpm > 0;
        if (racing) renderTheme(in, nowMs, dtMs);
        else renderIdle(nowMs, dtMs);
        if (racing) renderLiveAlerts(in, nowMs);
        renderEventAlerts(nowMs);
        if (racing && (settings.alerts & ALERT_LIMITER) && in.limiter)
        {
            // As before: the whole strip flashes red, 50 ms on and 50 off.
            if (nowMs - limiterFlashMs > 50) { limiterOn = !limiterOn; limiterFlashMs = nowMs; }
            fill(limiterOn ? RED : BLACK);
        }
        renderLayoutTest(nowMs);
    }

    // ---- Geometry (also used by the tests and the previews) ----
    Zone zone(int i) const { return zones[i]; }
    // Sides: height from the bottom, 0-1. Top: -1.
    float sideHeight(int i) const { return heights[i]; }
    // Top: -1 at the left corner, +1 at the right one. Sides: -1 or +1.
    float horizontal(int i) const { return xs[i]; }
    // 0 at the bottom end of the strip half the LED is on, 1 at the top centre.
    float halfPosition(int i) const { return halfPos[i]; }
    // Direction from the centre of the screen, radians (the gap at the bottom closed up).
    float angle(int i) const { return angles[i]; }

private:
    static constexpr int MAX_PARTICLES = 16;
    static constexpr float PI_F = 3.14159265f;
    static constexpr float ANGLE_GAP = 10.0f * PI_F / 180.0f;

    Layout layout_;
    int n = 0;
    int firstSide = 0, topCount = 0, lastSide = 0; // in strip order, sides of the effects
    int halfCount[2] = {0, 0};                     // LEDs from each end up to the top centre
    float halfLength[2] = {1, 1};
    Zone zones[MAX_LEDS];
    float heights[MAX_LEDS], xs[MAX_LEDS], halfPos[MAX_LEDS], angles[MAX_LEDS];
    float angleLo = 0, angleHi = 1; // directions of the two bottom ends of the frame

    Rgb *frame = nullptr;
    bool started = false;
    uint32_t lastFrameMs = 0;
    uint16_t seed = 1337;

    uint8_t flicker[20];
    uint32_t flickerMs = 0;
    bool flickerReady = false;

    bool limiterOn = false;
    uint32_t limiterFlashMs = 0;

    float deltaShown = 0;
    bool deltaShownValid = false;
    float gPeak = 1.0f;

    struct Particle
    {
        bool alive;
        float pos, speed, length;
        uint8_t bright;
    };
    Particle particles[2][MAX_PARTICLES];
    float spawnCarry[2] = {0, 0};
    uint8_t heat[2][MAX_LEDS];
    uint32_t fireMs = 0;

    uint32_t bestLapMs = 0, sectorMs = 0, fuelMs = 0, finalLapMs = 0, finishMs = 0;
    Rgb sectorColor = PURPLE;
    bool testActive = false;
    uint32_t testUntilMs = 0;

    // ---- helpers ----
    static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
    static float absf(float v) { return v < 0 ? -v : v; }
    uint8_t random8()
    {
        seed = static_cast<uint16_t>(seed * 2053u + 13849u);
        return static_cast<uint8_t>((seed & 0xFF) + (seed >> 8));
    }
    uint8_t random8(uint8_t lo, uint8_t hi) { return static_cast<uint8_t>(lo + ((random8() * static_cast<uint16_t>(hi - lo)) >> 8)); }
    void fill(Rgb c) { for (int i = 0; i < n; ++i) frame[i] = c; }
    // How much of the LED centred at `pos` (0-1 along a run of `count` LEDs)
    // lies below `level`: 255 inside, 0 outside, partial on the edge.
    static uint8_t coverage(float pos, float level, int count)
    {
        return unit8((level - pos) * count + 0.5f);
    }
    // The same on the top, from the centre (0) to the corners (1): the LED
    // in the middle of an odd top is lit only as the level grows.
    uint8_t topCoverage(float d, float level) const
    {
        const float w = 1.0f / (topCount > 0 ? topCount : 1);
        const float lo = d - w > 0.0f ? d - w : 0.0f;
        return unit8((level - lo) / (d + w - lo));
    }
    // Ages of the events in ms, or -1 when not running.
    static int32_t age(uint32_t startMs, uint32_t nowMs, uint32_t durationMs)
    {
        if (!startMs) return -1;
        const uint32_t a = nowMs - startMs;
        return a < durationMs ? static_cast<int32_t>(a) : -1;
    }

    void computeGeometry()
    {
        n = layout_.count();
        if (n > MAX_LEDS) n = MAX_LEDS;
        int right = layout_.right, top = layout_.top, left = layout_.left;
        if (right + left == 0) { right = left = n / 4; top = n - right - left; }
        const bool mirror = layout_.startLeft;
        firstSide = mirror ? left : right;
        lastSide = mirror ? right : left;
        topCount = top;
        halfLength[0] = firstSide + top * 0.5f;
        halfLength[1] = lastSide + top * 0.5f;
        halfCount[0] = halfCount[1] = 0;
        const float sideLength = right > left ? right : left > 0 ? left : 1;
        float px[MAX_LEDS], py[MAX_LEDS];
        for (int i = 0; i < n; ++i)
        {
            const float p = i + 0.5f;
            if (i < firstSide)
            {
                zones[i] = mirror ? Zone::Left : Zone::Right;
                heights[i] = p / firstSide;
                xs[i] = mirror ? -1.0f : 1.0f;
            }
            else if (i < firstSide + top)
            {
                zones[i] = Zone::Top;
                heights[i] = -1.0f;
                const float a = (p - firstSide) / top;
                xs[i] = mirror ? -1.0f + 2.0f * a : 1.0f - 2.0f * a;
            }
            else
            {
                zones[i] = mirror ? Zone::Right : Zone::Left;
                heights[i] = (n - p) / lastSide;
                xs[i] = mirror ? 1.0f : -1.0f;
            }
            if (p < halfLength[0]) { halfPos[i] = p / halfLength[0]; halfCount[0]++; }
            else { halfPos[i] = (n - p) / halfLength[1]; halfCount[1]++; }
            // Physical position in LED pitches, centre of the screen at the origin.
            px[i] = xs[i] * top * 0.5f;
            py[i] = (zones[i] == Zone::Top ? 1.0f : heights[i]) * sideLength - sideLength * 0.5f;
        }
        // Angles counterclockwise from the bottom of the right side; the
        // empty bottom edge shrinks to a small gap so that the bottom ends
        // light up when the load points down.
        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; ++i)
        {
            float a = atan2f(py[i], px[i]);
            if (a < -PI_F * 0.5f) a += 2.0f * PI_F;
            angles[i] = a;
            if (a < lo) lo = a;
            if (a > hi) hi = a;
        }
        angleLo = lo;
        angleHi = hi > lo ? hi : lo + 0.001f;
        for (int i = 0; i < n; ++i) angles[i] = closedAngle(angles[i]);
    }

    // A direction from the centre of the screen on the frame with the bottom
    // gap closed up: the frame takes all of the circle but ANGLE_GAP, the
    // directions that point between the two bottom ends share that gap.
    float closedAngle(float a) const
    {
        while (a < angleLo) a += 2.0f * PI_F;
        while (a >= angleLo + 2.0f * PI_F) a -= 2.0f * PI_F;
        if (a <= angleHi)
            return -PI_F * 0.5f + ANGLE_GAP + (a - angleLo) / (angleHi - angleLo) * (2.0f * PI_F - 2.0f * ANGLE_GAP);
        return 1.5f * PI_F - ANGLE_GAP + (a - angleHi) / (angleLo + 2.0f * PI_F - angleHi) * 2.0f * ANGLE_GAP;
    }

    void updateFlicker(uint32_t nowMs)
    {
        if (flickerReady && nowMs - flickerMs <= 30) return;
        for (int k = 0; k < 20; ++k) flicker[k] = random8(static_cast<uint8_t>(255 - settings.gurgle), 255);
        flickerMs = nowMs;
        flickerReady = true;
    }
    int gurgleCount() const { return settings.gurgle / 15 + 1; }

    // ---- race themes ----
    void renderTheme(const Input &in, uint32_t nowMs, uint32_t dtMs)
    {
        const float maxRpm = in.maxRpm;
        const float rpm = in.rpm > maxRpm ? maxRpm : in.rpm;
        switch (settings.theme)
        {
        case THEME_DEFAULT: classicFill(rpm, maxRpm, false); break;
        case THEME_DEFAULT_REVERSE: classicFill(rpm, maxRpm, true); break;
        case THEME_F1_CENTER: classicSymmetric(rpm, maxRpm, true); break;
        case THEME_F1_REVERSE: classicSymmetric(rpm, maxRpm, false); break;
        case THEME_SUPERCAR: supercar(rpm, maxRpm); break;
        case THEME_SMOOTH_FADE:
        {
            const float pct = rpm / maxRpm;
            fill(hsv(static_cast<uint8_t>(140 - static_cast<uint8_t>(pct * 140)), 255, 255));
            break;
        }
        case THEME_DELTA: deltaTheme(in, nowMs, dtMs); break;
        case THEME_PEDALS: pedalsTheme(in); break;
        case THEME_SPEED: speedTheme(in, dtMs); break;
        case THEME_G_FORCE: gForceTheme(in, dtMs); break;
        case THEME_TYRES: tyresTheme(in, nowMs); break;
        case THEME_AMBIENT: fill(isfinite(in.timeOfDayH) ? scaled(skyColor(in.timeOfDayH), 200) : scaled(settings.idleColor, 128)); break;
        case THEME_ALERTS_ONLY: fill(scaled(settings.idleColor, 90)); break;
        default: classicFill(rpm, maxRpm, false); break;
        }
    }

    // DEFAULT / DEFAULT REVERSE: fills along the strip, green to 60%, yellow
    // to 85%, then red.
    void classicFill(float rpm, float maxRpm, bool reverse)
    {
        const int lit = static_cast<int>((rpm / maxRpm) * n);
        const int greenEnd = static_cast<int>(ceil(n * 0.6)), yellowEnd = static_cast<int>(ceil(n * 0.85));
        for (int i = 0; i < n && i < lit; ++i)
            frame[reverse ? n - 1 - i : i] = i < greenEnd ? GREEN : i < yellowEnd ? YELLOW : RED;
        if (settings.gurgle == 0 || rpm <= 0) return;
        for (int k = 0; k < gurgleCount(); ++k)
        {
            const int i = lit - 1 - k;
            if (i < 0 || i >= n) continue;
            frame[reverse ? n - 1 - i : i] = scaled(i < greenEnd ? GREEN : i < yellowEnd ? YELLOW : RED, flicker[k % 20]);
        }
    }

    // F1 CENTER (from the middle of the strip) and F1 REVERSE (from both ends).
    void classicSymmetric(float rpm, float maxRpm, bool fromCenter)
    {
        const int center = n / 2;
        const int perSide = static_cast<int>((rpm / maxRpm) * center);
        const int greenEnd = static_cast<int>(ceil(center * 0.35)), yellowEnd = static_cast<int>(ceil(center * 0.75));
        for (int i = 0; i < perSide; ++i)
        {
            const Rgb c = i < greenEnd ? GREEN : i < yellowEnd ? YELLOW : RED;
            if (fromCenter)
            {
                if (center + i < n) frame[center + i] = c;
                if (center - 1 - i >= 0) frame[center - 1 - i] = c;
            }
            else
            {
                if (i < n) frame[i] = c;
                if (n - 1 - i >= 0) frame[n - 1 - i] = c;
            }
        }
        if (settings.gurgle == 0 || rpm <= 0) return;
        for (int k = 0; k < gurgleCount(); ++k)
        {
            const int v = perSide - 1 - k;
            if (v < 0 || v >= center) continue;
            const Rgb c = v < greenEnd ? GREEN : v < yellowEnd ? YELLOW : RED;
            const Rgb left = scaled(c, flicker[k % 20]), right = scaled(c, flicker[(k + 1) % 20]);
            if (fromCenter)
            {
                if (center + v < n) frame[center + v] = right;
                if (center - 1 - v >= 0) frame[center - 1 - v] = left;
            }
            else
            {
                if (v < n) frame[v] = left;
                if (n - 1 - v >= 0) frame[n - 1 - v] = right;
            }
        }
    }
    // SUPERCAR: all blue, only above 70% of the revs.
    void supercar(float rpm, float maxRpm)
    {
        const float threshold = maxRpm * 0.70f;
        if (rpm <= threshold) return;
        const int lit = static_cast<int>(((rpm - threshold) / (maxRpm - threshold)) * n);
        for (int i = 0; i < lit && i < n; ++i) frame[i] = BLUE;
        if (settings.gurgle == 0) return;
        for (int k = 0; k < gurgleCount(); ++k)
        {
            const int i = lit - 1 - k;
            if (i >= 0 && i < n) frame[i] = scaled(BLUE, flicker[k % 20]);
        }
    }

    // Revs on the top, from the centre to the corners: the range of the
    // screen's shift lights (25% below the game's alert to the maximum).
    void revsOnTop(const Input &in)
    {
        const float pct = in.rpm / in.maxRpm * 100.0f;
        const bool alertValid = in.alertRpm > 0 && in.alertRpm < in.maxRpm;
        const float start = alertValid ? clampf(in.alertRpm / in.maxRpm * 100.0f - 25.0f, 30.0f, 90.0f) : 65.0f;
        const float level = clampf((pct - start) / (100.0f - start), 0.0f, 1.0f);
        const float perHalf = topCount * 0.5f;
        const int tip = static_cast<int>(level * perHalf + 0.5f); // LEDs lit on each half
        for (int i = 0; i < n; ++i)
        {
            if (zones[i] != Zone::Top) continue;
            const float d = absf(xs[i]);
            const uint8_t c = topCoverage(d, level);
            if (!c) continue;
            Rgb color = d < 0.4f ? SHIFT_GREEN : d < 0.7f ? SHIFT_YELLOW : SHIFT_RED;
            // The flicker of the tip, as on the classic bars.
            if (settings.gurgle > 0)
            {
                const int fromTip = tip - 1 - static_cast<int>(d * perHalf);
                if (fromTip >= 0 && fromTip < gurgleCount()) color = scaled(color, flicker[(fromTip + (xs[i] < 0)) % 20]);
            }
            frame[i] = scaled(color, c);
        }
    }

    // A bar on each side growing from the bottom (level 0-1).
    void sideBars(float level, Rgb color, bool right, bool left)
    {
        for (int i = 0; i < n; ++i)
        {
            const Zone z = zones[i];
            if (z == Zone::Top || (z == Zone::Right && !right) || (z == Zone::Left && !left)) continue;
            const int side = (z == Zone::Right) != layout_.startLeft ? firstSide : lastSide;
            const uint8_t c = coverage(heights[i], level, side);
            if (c) frame[i] = scaled(color, c);
        }
    }

    // DELTA: sides green when ahead of the best lap, red when behind, as
    // high as the delta against the scale; revs on the top.
    void deltaTheme(const Input &in, uint32_t nowMs, uint32_t dtMs)
    {
        revsOnTop(in);
        if (!in.hasDelta)
        {
            // No reference lap yet: a slow blue glow at the bottom of the sides.
            deltaShownValid = false;
            const uint8_t glow = static_cast<uint8_t>(30 + scale8(sin8(static_cast<uint8_t>(nowMs / 12)), 70));
            sideBars(0.18f, scaled(Rgb(0, 60, 255), glow), true, true);
            return;
        }
        // About 0.12 s of smoothing: the delta moves every packet.
        if (!deltaShownValid) { deltaShown = in.deltaS; deltaShownValid = true; }
        else deltaShown += (in.deltaS - deltaShown) * clampf(dtMs / 120.0f, 0.0f, 1.0f);
        const float scale = settings.deltaScaleS > 0.05f ? settings.deltaScaleS : 0.05f;
        const float level = 0.06f + 0.94f * clampf(absf(deltaShown) / scale, 0.0f, 1.0f);
        sideBars(level, deltaShown <= 0.0f ? AHEAD : BEHIND, true, true);
    }

    // PEDALS: throttle on the right, brake on the left, revs on the top.
    void pedalsTheme(const Input &in)
    {
        revsOnTop(in);
        sideBars(clampf(in.throttle, 0.0f, 1.0f), SHIFT_GREEN, true, false);
        sideBars(clampf(in.brake, 0.0f, 1.0f), RED, false, true);
    }

    // SPEED: streaks of light leave the top centre and run down the sides,
    // faster and denser with the speed, coloured by the revs.
    void speedTheme(const Input &in, uint32_t dtMs)
    {
        const float speed = clampf(in.speedKmh / 300.0f, 0.0f, 1.2f);
        const float revs = clampf(in.rpm / in.maxRpm, 0.0f, 1.0f);
        const uint8_t hue = static_cast<uint8_t>(160 - static_cast<int>(revs * 160));
        const Rgb glow = hsv(hue, 255, static_cast<uint8_t>(12 + speed * 40));
        fill(glow);
        const Rgb streak = hsv(hue, 190, 255);
        const float dt = dtMs / 1000.0f;
        for (int h = 0; h < 2; ++h)
        {
            Particle *list = particles[h];
            spawnCarry[h] += dt * speed * 16.0f;
            while (spawnCarry[h] >= 1.0f)
            {
                spawnCarry[h] -= 1.0f;
                for (int k = 0; k < MAX_PARTICLES; ++k)
                {
                    if (list[k].alive) continue;
                    list[k].alive = true;
                    list[k].pos = 1.0f;
                    list[k].speed = 0.8f + random8() / 560.0f;
                    list[k].length = 0.05f + 0.10f * (speed > 1.0f ? 1.0f : speed);
                    list[k].bright = random8(140, 255);
                    break;
                }
            }
            const int count = halfCount[h];
            const float length = halfLength[h];
            for (int k = 0; k < MAX_PARTICLES; ++k)
            {
                Particle &p = list[k];
                if (!p.alive) continue;
                p.pos -= 1.7f * speed * p.speed * dt;
                if (p.pos + p.length < 0.0f) { p.alive = false; continue; }
                // The head is the lowest point, the tail fades towards the centre.
                const int from = static_cast<int>(p.pos * length - 0.5f), to = static_cast<int>((p.pos + p.length) * length + 0.5f);
                for (int j = from < 0 ? 0 : from; j <= to && j < count; ++j)
                {
                    const float s = (j + 0.5f) / length;
                    const float f = (s - p.pos) / p.length;
                    if (f < -0.5f / (p.length * length) || f > 1.0f) continue;
                    const uint8_t v = scale8(p.bright, unit8(1.0f - clampf(f, 0.0f, 1.0f)));
                    const int i = h == 0 ? j : n - 1 - j;
                    frame[i] = added(frame[i], scaled(streak, v));
                }
            }
        }
    }

    // G-FORCE: a light on the frame where the load pushes the body: on the
    // left in a right turn, on the top under braking, at the bottom of the
    // sides when accelerating. Green to red with the share of the car's grip.
    void gForceTheme(const Input &in, uint32_t dtMs)
    {
        const float lat = in.lateralG, lon = in.longitudinalG;
        const float g = sqrtf(lat * lat + lon * lon);
        // The reference follows the car: the highest load seen, slowly forgotten.
        gPeak -= dtMs * 0.00005f;
        if (gPeak < 0.9f) gPeak = 0.9f;
        if (g > gPeak) gPeak = g;
        const float level = clampf(g / gPeak, 0.0f, 1.0f);
        const Rgb color = hsv(static_cast<uint8_t>(96 - static_cast<int>(level * 96)), 255, 255);
        fill(scaled(color, 14));
        if (g < 0.03f) return;
        const float target = closedAngle(atan2f(-lon, -lat));
        const float width = 0.55f + 0.5f * level;
        for (int i = 0; i < n; ++i)
        {
            float d = absf(angles[i] - target);
            if (d > PI_F) d = 2.0f * PI_F - d;
            if (d >= width) continue;
            const float k = 1.0f - (d / width) * (d / width);
            frame[i] = added(frame[i], scaled(color, unit8(k * k * (0.25f + 0.75f * level))));
        }
    }

    // TYRES: each corner in the colour of its tyre's temperature (blue cold,
    // green right, red hot), blended along the frame. A wheel that locks or
    // spins flashes on its side.
    void tyresTheme(const Input &in, uint32_t nowMs)
    {
        Rgb c[4];
        for (int w = 0; w < 4; ++w) c[w] = tyreColor(in.tyreTemp[w]);
        for (int i = 0; i < n; ++i)
        {
            if (zones[i] == Zone::Top) frame[i] = blended(c[0], c[1], unit8((xs[i] + 1.0f) * 0.5f));
            else if (zones[i] == Zone::Right) frame[i] = blended(c[3], c[1], unit8(heights[i]));
            else frame[i] = blended(c[2], c[0], unit8(heights[i]));
        }
        wheelAlerts(in, nowMs, true);
    }

    // ---- alerts ----
    void renderLiveAlerts(const Input &in, uint32_t nowMs)
    {
        if ((settings.alerts & ALERT_GRIP) && settings.theme != THEME_TYRES) wheelAlerts(in, nowMs, false);
        if (settings.alerts & ALERT_BRAKING) brakingAlert(in, nowMs);
        if ((settings.alerts & ALERT_GEAR) && in.suggestedGear >= 1 && in.suggestedGear <= 9 && in.gear >= 1 &&
            in.suggestedGear < in.gear && in.speedKmh > 30.0f && (nowMs / 90) % 2 == 0)
        {
            for (int i = 0; i < n; ++i)
                if (zones[i] == Zone::Top && absf(xs[i]) > 0.82f) frame[i] = CYAN;
        }
    }

    // Front wheels on the upper half of their side, rear wheels on the lower
    // half: red for a lock-up, orange for wheelspin. `flash` blinks them.
    void wheelAlerts(const Input &in, uint32_t nowMs, bool flash)
    {
        if (flash && (nowMs / 60) % 2) return;
        for (int w = 0; w < 4; ++w)
        {
            const uint8_t state = in.wheel[w];
            if (state != WHEEL_LOCKING && state != WHEEL_SPINNING) continue;
            const Zone side = w % 2 == 0 ? Zone::Left : Zone::Right;
            const bool front = w < 2;
            const Rgb color = state == WHEEL_LOCKING ? RED : ORANGE;
            for (int i = 0; i < n; ++i)
                if (zones[i] == side && (heights[i] >= 0.5f) == front) frame[i] = blended(frame[i], color, 230);
        }
    }

    // Approaching the braking point of the best lap: from 150 m the sides
    // light up from the top down, white, then amber (100 m) and red (50 m).
    // It goes away as soon as the brake pedal is pressed.
    void brakingAlert(const Input &in, uint32_t nowMs)
    {
        (void)nowMs;
        if (!in.hasBrakePoint || in.brake > 0.25f || in.speedKmh < 60.0f) return;
        const float m = in.brakePointM;
        if (m < 0.0f || m > 150.0f) return;
        const float level = m < 8.0f ? 1.0f : 1.0f - m / 150.0f;
        const Rgb color = m > 100.0f ? Rgb(200, 200, 200) : m > 50.0f ? AMBER : RED;
        for (int i = 0; i < n; ++i)
        {
            if (zones[i] == Zone::Top) continue;
            const int side = (zones[i] == Zone::Right) != layout_.startLeft ? firstSide : lastSide;
            const uint8_t c = coverage(1.0f - heights[i], level, side);
            if (c) frame[i] = blended(frame[i], color, c);
        }
    }

    void renderEventAlerts(uint32_t nowMs)
    {
        const uint16_t alerts = settings.alerts;
        int32_t a;
        if ((alerts & ALERT_FUEL) && (a = age(fuelMs, nowMs, 1800)) >= 0)
        {
            // Three amber pulses on the lower half of the sides.
            const uint8_t v = pulse(a % 600, 600);
            for (int i = 0; i < n; ++i)
                if (zones[i] != Zone::Top && heights[i] < 0.5f) frame[i] = blended(frame[i], AMBER, v);
        }
        if ((alerts & ALERT_SECTORS) && (a = age(sectorMs, nowMs, 1200)) >= 0)
        {
            const uint8_t v = a < 800 ? 255 : unit8(1.0f - (a - 800) / 400.0f);
            for (int i = 0; i < n; ++i)
                if (zones[i] == Zone::Top) frame[i] = blended(frame[i], sectorColor, v);
        }
        if ((alerts & ALERT_FLAGS) && (a = age(finalLapMs, nowMs, 1500)) >= 0)
        {
            // White flag: three white flashes.
            if ((a / 250) % 2 == 0) fill(WHITE);
        }
        if ((alerts & ALERT_FLAGS) && (a = age(finishMs, nowMs, 5000)) >= 0)
        {
            // Chequered flag waving along the strip.
            const int shift = a / 120;
            for (int i = 0; i < n; ++i) frame[i] = ((i + shift) / 4) % 2 ? WHITE : BLACK;
        }
        if ((alerts & ALERT_BEST_LAP) && (a = age(bestLapMs, nowMs, 1800)) >= 0)
        {
            // Purple, the colour of the best time, three times.
            const uint8_t v = pulse(a % 600, 600);
            for (int i = 0; i < n; ++i) frame[i] = blended(frame[i], PURPLE, v);
        }
    }
    // 0-255-0 in `period` ms: up quickly, a hold, down.
    static uint8_t pulse(int32_t t, int32_t period)
    {
        const float f = static_cast<float>(t) / period;
        if (f < 0.15f) return unit8(f / 0.15f);
        if (f < 0.55f) return 255;
        if (f < 0.8f) return unit8(1.0f - (f - 0.55f) / 0.25f);
        return 0;
    }

    void renderLayoutTest(uint32_t nowMs)
    {
        if (!layoutTestActive(nowMs)) { testActive = false; return; }
        for (int i = 0; i < n; ++i)
        {
            // The real sides of the layout, not the quarters of a plain bar.
            const int first = layout_.startLeft ? layout_.left : layout_.right;
            const int last = layout_.startLeft ? layout_.right : layout_.left;
            Zone z = Zone::Top;
            if (i < first) z = layout_.startLeft ? Zone::Left : Zone::Right;
            else if (i >= n - last) z = layout_.startLeft ? Zone::Right : Zone::Left;
            frame[i] = z == Zone::Right ? Rgb(255, 0, 0) : z == Zone::Top ? Rgb(0, 255, 0) : Rgb(0, 0, 255);
        }
        if ((nowMs / 250) % 2 == 0 && n > 0) frame[0] = WHITE;
    }

    // ---- idle animations ----
    void renderIdle(uint32_t nowMs, uint32_t dtMs)
    {
        const Rgb c = settings.idleColor;
        switch (settings.idleMode)
        {
        case IDLE_OFF: break;
        case IDLE_SOLID: fill(c); break;
        case IDLE_BREATHING:
        {
            // FastLED beatsin8(20, 20, 255): 20 breaths a minute.
            const uint16_t beat = static_cast<uint16_t>((nowMs * 5120u * 280u) >> 16);
            const uint8_t v = static_cast<uint8_t>(20 + scale8(sin8(static_cast<uint8_t>(beat >> 8)), 235));
            fill(scaled(c, v));
            break;
        }
        case IDLE_RAINBOW:
        {
            // FastLED fill_rainbow(leds, n, millis() / 10, 255 / n).
            uint8_t hue = static_cast<uint8_t>(nowMs / 10);
            const uint8_t delta = static_cast<uint8_t>(255 / (n > 0 ? n : 1));
            for (int i = 0; i < n; ++i, hue = static_cast<uint8_t>(hue + delta)) frame[i] = hsv(hue, 240, 255);
            break;
        }
        case IDLE_WAVE: waveIdle(nowMs); break;
        case IDLE_FIRE: fireIdle(nowMs, dtMs); break;
        case IDLE_AURORA: auroraIdle(nowMs); break;
        case IDLE_TV: fill(WHITE); break;
        default: fill(c); break;
        }
    }

    // Two comets of the idle colour climb from the bottom ends and meet at the
    // top centre, which flashes.
    void waveIdle(uint32_t nowMs)
    {
        const Rgb c = settings.idleColor;
        fill(scaled(c, 14));
        const float phase = (nowMs % 2600) / 2600.0f;
        if (phase < 0.8f)
        {
            const float x = phase / 0.8f;
            const float head = x * x * (3.0f - 2.0f * x); // smoothstep
            for (int i = 0; i < n; ++i)
            {
                const float behind = head - halfPos[i];
                if (behind < -0.02f || behind > 0.3f) continue;
                const float k = behind < 0.0f ? 1.0f + behind / 0.02f : 1.0f - behind / 0.3f;
                frame[i] = added(frame[i], scaled(c, unit8(k * k)));
            }
        }
        else
        {
            const float k = 1.0f - (phase - 0.8f) / 0.2f;
            for (int i = 0; i < n; ++i)
                if (halfPos[i] > 0.8f) frame[i] = added(frame[i], scaled(c, unit8(k * (halfPos[i] - 0.8f) * 5.0f)));
        }
    }

    // Fire2012 by Mark Kriegsman on each half: flames rise from the bottom of
    // the sides towards the top centre.
    void fireIdle(uint32_t nowMs, uint32_t dtMs)
    {
        (void)dtMs;
        // The simulation was tuned for 60 frames a second.
        int steps = static_cast<int>((nowMs - fireMs) / 16);
        if (steps > 4) steps = 4;
        if (steps > 0) fireMs = nowMs;
        for (int s = 0; s < steps; ++s)
            for (int h = 0; h < 2; ++h)
            {
                const int count = halfCount[h];
                if (count < 3) continue;
                uint8_t *cells = heat[h];
                for (int i = 0; i < count; ++i) cells[i] = qsub8(cells[i], random8(0, static_cast<uint8_t>(550 / count + 2)));
                for (int k = count - 1; k >= 2; --k) cells[k] = static_cast<uint8_t>((cells[k - 1] + cells[k - 2] + cells[k - 2]) / 3);
                if (random8() < 120)
                {
                    const int y = random8(0, 7) % count;
                    cells[y] = qadd8(cells[y], random8(160, 255));
                }
            }
        for (int i = 0; i < halfCount[0]; ++i) frame[i] = heatColor(heat[0][i]);
        for (int j = 0; j < halfCount[1]; ++j) frame[n - 1 - j] = heatColor(heat[1][j]);
    }

    // Slow waves of green, teal and purple.
    void auroraIdle(uint32_t nowMs)
    {
        const int count = n > 0 ? n : 1;
        for (int i = 0; i < n; ++i)
        {
            const uint8_t v1 = sin8(static_cast<uint8_t>(i * 384 / count + nowMs / 23));
            const uint8_t v2 = sin8(static_cast<uint8_t>(i * 832 / count - nowMs / 41));
            const uint8_t v3 = sin8(static_cast<uint8_t>(i * 160 / count + nowMs / 67));
            const uint8_t hue = static_cast<uint8_t>(80 + ((v1 * 110) >> 8) + (v3 >> 3));
            const uint8_t val = static_cast<uint8_t>(40 + scale8(scale8(v2, v2), 215));
            frame[i] = hsv(hue, 240, val);
        }
    }
};
} // namespace LedFx
