// Tests of src/LedEffects.h, the effects of the LED strip:
//   g++ -std=c++11 -Wall -Wextra -pedantic firmware/led-strip/tests/led_effects.cpp -o /tmp/led-tests && /tmp/led-tests
#include "../src/LedEffects.h"
#include <stdio.h>
#include <stdlib.h>

using namespace LedFx;

static int failures = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } \
    } while (0)

static Rgb frame[MAX_LEDS];

static bool same(Rgb a, Rgb b) { return a == b; }
static int brightness(Rgb c) { return c.r + c.g + c.b; }
static bool isBlack(Rgb c) { return c.r == 0 && c.g == 0 && c.b == 0; }

static Input racing(float rpm = 4000, float maxRpm = 8000)
{
    Input in;
    in.racing = true;
    in.rpm = rpm;
    in.maxRpm = maxRpm;
    in.alertRpm = 7000;
    in.speedKmh = 150;
    return in;
}

// ---- The six original themes, as main.cpp drew them before this version ----
static void originalTheme(int theme, float rpm, float maxRPM, int activeLeds, Rgb *leds)
{
    if (rpm > maxRPM) rpm = maxRPM;
    for (int i = 0; i < activeLeds; ++i) leds[i] = BLACK;
    switch (theme)
    {
    case THEME_DEFAULT: {
        int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
        for (int i = 0; i < activeLeds; i++) {
            if (i < numLedsToLight) {
                if (i < (activeLeds * 0.6)) leds[i] = GREEN;
                else if (i < (activeLeds * 0.85)) leds[i] = YELLOW;
                else leds[i] = RED;
            } else leds[i] = BLACK;
        }
        break;
    }
    case THEME_DEFAULT_REVERSE: {
        int numLedsToLight = (int)((rpm / maxRPM) * activeLeds);
        for (int i = 0; i < activeLeds; i++) {
            if (i < numLedsToLight) {
                if (i < (activeLeds * 0.6)) leds[activeLeds - 1 - i] = GREEN;
                else if (i < (activeLeds * 0.85)) leds[activeLeds - 1 - i] = YELLOW;
                else leds[activeLeds - 1 - i] = RED;
            } else leds[activeLeds - 1 - i] = BLACK;
        }
        break;
    }
    case THEME_F1_CENTER: {
        int center = activeLeds / 2;
        int ledsPerSide = (int)((rpm / maxRPM) * center);
        for (int i = 0; i < ledsPerSide; i++) {
            Rgb color;
            if (i < center * 0.35) color = GREEN;
            else if (i < center * 0.75) color = YELLOW;
            else color = RED;
            if (center + i < activeLeds) leds[center + i] = color;
            if (center - 1 - i >= 0) leds[center - 1 - i] = color;
        }
        break;
    }
    case THEME_F1_REVERSE: {
        int center = activeLeds / 2;
        int ledsPerSide = (int)((rpm / maxRPM) * center);
        for (int i = 0; i < ledsPerSide; i++) {
            Rgb color;
            if (i < center * 0.35) color = GREEN;
            else if (i < center * 0.75) color = YELLOW;
            else color = RED;
            if (i < activeLeds) leds[i] = color;
            if (activeLeds - 1 - i >= 0) leds[activeLeds - 1 - i] = color;
        }
        break;
    }
    case THEME_SUPERCAR: {
        float threshold = maxRPM * 0.70;
        if (rpm > threshold) {
            float activeRange = maxRPM - threshold;
            float currentActive = rpm - threshold;
            int numLedsToLight = (int)((currentActive / activeRange) * activeLeds);
            for (int i = 0; i < numLedsToLight; i++) leds[i] = BLUE;
        }
        break;
    }
    case THEME_SMOOTH_FADE: {
        float pct = rpm / maxRPM;
        uint8_t hue = 140 - (uint8_t)(pct * 140);
        for (int i = 0; i < activeLeds; ++i) leds[i] = hsv(hue, 255, 255);
        break;
    }
    }
}

static void testColourCode()
{
    // Reference values of FastLED's code.
    CHECK(sin8(0) == 128);
    CHECK(sin8(64) == 255);
    CHECK(sin8(192) == 1);
    CHECK(same(hsv(0, 255, 255), Rgb(255, 0, 0)));
    CHECK(same(hsv(64, 255, 255), Rgb(171, 170, 0)));
    CHECK(same(hsv(96, 255, 255), Rgb(0, 255, 0)));
    CHECK(same(hsv(160, 255, 255), Rgb(0, 0, 255)));
    CHECK(same(hsv(0, 0, 255), Rgb(255, 255, 255)));
    CHECK(isBlack(hsv(10, 255, 0)));
    CHECK(scale8(255, 255) == 255 && scale8(255, 0) == 0 && scale8(100, 127) == 50);
    CHECK(same(heatColor(0), Rgb(0, 0, 0)));
    CHECK(same(heatColor(255), Rgb(255, 255, 252)));
    CHECK(same(blended(Rgb(0, 0, 0), Rgb(255, 255, 255), 255), Rgb(255, 255, 255)));
    CHECK(same(blended(Rgb(10, 20, 30), Rgb(255, 255, 255), 0), Rgb(10, 20, 30)));
    // Tyres: blue cold, green right, red hot, dark without data.
    CHECK(tyreColor(40).b > tyreColor(40).g);
    CHECK(tyreColor(88).g > 150 && tyreColor(88).r < 30);
    CHECK(tyreColor(120).r > 200 && tyreColor(120).g < 60);
    CHECK(brightness(tyreColor(NAN)) < 80);
    // Sky: dark at night, warm at sunset, bright at noon.
    CHECK(brightness(skyColor(2)) < 100);
    CHECK(skyColor(19).r > 200 && skyColor(19).b < 60);
    CHECK(brightness(skyColor(13)) > 500);
}

static void testLayout()
{
    const Layout a = Layout::forCount(143);
    CHECK(a.right == 38 && a.top == 67 && a.left == 38 && !a.startLeft);
    const Layout b = Layout::forCount(150);
    CHECK(b.right == 40 && b.top == 70 && b.left == 40);
    const Layout c = a.resized(100);
    CHECK(c.count() == 100 && c.right == 27 && c.left == 27 && c.top == 46);
    Layout d;
    d.right = 30; d.top = 80; d.left = 33;
    CHECK(d.resized(143).count() == 143);
    CHECK(Layout::forCount(1).count() == 1);
    CHECK(Layout::forCount(500).count() == MAX_LEDS);
    Layout bad;
    bad.right = -1;
    CHECK(!bad.valid());
}

static void testGeometry()
{
    Renderer r;
    Layout l = Layout::forCount(143); // 38 + 67 + 38
    r.setLayout(l);
    CHECK(r.count() == 143);
    // From the bottom right, up, along the top to the left, down.
    CHECK(r.zone(0) == Zone::Right && r.sideHeight(0) < 0.05f);
    CHECK(r.zone(37) == Zone::Right && r.sideHeight(37) > 0.95f);
    CHECK(r.zone(38) == Zone::Top && r.horizontal(38) > 0.95f);
    CHECK(r.zone(71) == Zone::Top && fabsf(r.horizontal(71)) < 0.02f);
    CHECK(r.zone(104) == Zone::Top && r.horizontal(104) < -0.95f);
    CHECK(r.zone(105) == Zone::Left && r.sideHeight(105) > 0.95f);
    CHECK(r.zone(142) == Zone::Left && r.sideHeight(142) < 0.05f);
    // Halves meet at the top centre.
    CHECK(r.halfPosition(0) < 0.02f && r.halfPosition(142) < 0.02f);
    CHECK(r.halfPosition(71) > 0.98f);
    // Angles: right side at the start, the top centre half way, left at the end.
    CHECK(r.angle(0) < r.angle(37) && r.angle(37) < r.angle(71) && r.angle(71) < r.angle(105));
    CHECK(fabsf(r.angle(71) - 1.5708f) < 0.05f);
    // Mirrored: the strip starts at the bottom left.
    l.startLeft = true;
    r.setLayout(l);
    CHECK(r.zone(0) == Zone::Left && r.zone(142) == Zone::Right);
    CHECK(r.horizontal(38) < -0.95f && r.horizontal(104) > 0.95f);
    CHECK(r.angle(0) > r.angle(142)); // the first LED is now on the left
    // A plain bar: the outer quarters work as sides.
    Layout bar;
    bar.right = 0; bar.top = 100; bar.left = 0;
    r.setLayout(bar);
    CHECK(r.count() == 100 && r.zone(0) == Zone::Right && r.zone(50) == Zone::Top && r.zone(99) == Zone::Left);
}

// The first six themes draw exactly what the previous firmware drew.
static void testClassicThemesUnchanged()
{
    static const int counts[] = {143, 150, 10, 37, 200, 1, 2};
    static const float rpms[] = {0, 900, 3500, 4801, 6400, 7399, 7400, 9000};
    Rgb expected[MAX_LEDS];
    for (int theme = 0; theme <= 5; ++theme)
        for (int count : counts)
            for (float rpm : rpms)
            {
                Renderer r;
                r.setLayout(Layout::forCount(count));
                r.settings.theme = static_cast<uint8_t>(theme);
                r.settings.gurgle = 0;
                r.settings.alerts = 0;
                Input in = racing(rpm, 7400);
                r.render(in, 1000, frame);
                originalTheme(theme, rpm, 7400, count, expected);
                bool equal = true;
                for (int i = 0; i < count; ++i) equal = equal && same(frame[i], expected[i]);
                if (!equal) printf("  theme %d, %d LED, %.0f rpm differs\n", theme, count, rpm);
                CHECK(equal);
            }
}

// With the flicker on, only the tip of the bar changes, and only darker.
static void testGurgle()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_DEFAULT;
    r.settings.gurgle = 150;
    r.settings.alerts = 0;
    Rgb expected[MAX_LEDS];
    r.render(racing(5000, 7400), 1000, frame);
    originalTheme(THEME_DEFAULT, 5000, 7400, 143, expected);
    const int lit = static_cast<int>(5000.0f / 7400.0f * 143);
    const int tip = 150 / 15 + 1;
    bool ok = true;
    for (int i = 0; i < 143; ++i)
    {
        if (i < lit - tip || i >= lit) ok = ok && same(frame[i], expected[i]);
        else ok = ok && brightness(frame[i]) <= brightness(expected[i]) && brightness(frame[i]) >= brightness(expected[i]) * 100 / 256 - 3;
    }
    CHECK(ok);
}

static void testDeltaTheme()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_DELTA;
    r.settings.alerts = 0;
    r.settings.deltaScaleS = 1.0f;
    Input in = racing(3000, 8000); // revs below the shift lights: the top stays dark
    in.hasDelta = true;
    in.deltaS = -0.5f; // ahead
    r.render(in, 1000, frame);
    // Half way up both sides, green; the top dark.
    CHECK(same(frame[0], AHEAD) && same(frame[142], AHEAD));
    CHECK(same(frame[15], AHEAD) && isBlack(frame[25]));
    CHECK(same(frame[127], AHEAD) && isBlack(frame[117]));
    bool topDark = true;
    for (int i = 38; i < 105; ++i) topDark = topDark && isBlack(frame[i]);
    CHECK(topDark);
    // Behind by more than the scale: the sides red to the top (after the smoothing).
    in.deltaS = 1.7f;
    for (uint32_t t = 1016; t < 1600; t += 16) r.render(in, t, frame);
    CHECK(same(frame[0], BEHIND) && same(frame[37], BEHIND) && same(frame[105], BEHIND));
    // No reference lap: a dim blue glow at the bottom only.
    in.hasDelta = false;
    r.render(in, 2000, frame);
    CHECK(frame[0].b > frame[0].r && frame[0].b > 0 && isBlack(frame[20]));
    // Revs at the top of the scale: the whole top is lit, red at the corners.
    in.rpm = 8000;
    r.settings.gurgle = 0;
    r.render(in, 2016, frame);
    CHECK(same(frame[38], SHIFT_RED) && same(frame[104], SHIFT_RED));
    CHECK(brightness(frame[71]) > 0 && frame[71].g > frame[71].r);
}

static void testPedalsTheme()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_PEDALS;
    r.settings.alerts = 0;
    Input in = racing(2000, 8000);
    in.throttle = 0.5f;
    in.brake = 1.0f;
    r.render(in, 1000, frame);
    // Throttle on the right side: 19 of 38 LEDs.
    int right = 0;
    for (int i = 0; i < 38; ++i) right += same(frame[i], SHIFT_GREEN);
    CHECK(right == 19);
    // Brake on the left side: all of it.
    bool left = true;
    for (int i = 105; i < 143; ++i) left = left && same(frame[i], RED);
    CHECK(left);
    // Mirrored mounting: throttle still on the physical right side.
    Layout l = Layout::forCount(143);
    l.startLeft = true;
    r.setLayout(l);
    r.render(in, 1016, frame);
    CHECK(same(frame[142], SHIFT_GREEN) && same(frame[0], RED));
}

static int brightestIndex()
{
    int best = 0;
    for (int i = 1; i < 143; ++i)
        if (brightness(frame[i]) > brightness(frame[best])) best = i;
    return best;
}

static void testGForceTheme()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_G_FORCE;
    r.settings.alerts = 0;
    Input in = racing();
    in.longitudinalG = -1.2f; // braking: the load goes to the top
    r.render(in, 1000, frame);
    const int top = brightestIndex();
    CHECK(top > 60 && top < 82);
    in.longitudinalG = 0;
    in.lateralG = 1.3f; // right turn: the body is pushed to the left
    r.render(in, 1016, frame);
    CHECK(r.zone(brightestIndex()) == Zone::Left);
    in.lateralG = 0;
    in.longitudinalG = 0.6f; // accelerating: the bottom of both sides
    r.render(in, 1032, frame);
    CHECK(brightness(frame[0]) > brightness(frame[37]) && brightness(frame[142]) > brightness(frame[105]));
    CHECK(brightness(frame[0]) > brightness(frame[71]));
}

static void testTyresTheme()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_TYRES;
    r.settings.alerts = 0;
    Input in = racing();
    in.tyreTemp[0] = 125; // front left very hot
    in.tyreTemp[1] = 85;
    in.tyreTemp[2] = 85;
    in.tyreTemp[3] = 40;  // rear right cold
    r.render(in, 1000, frame);
    CHECK(frame[104].r > 180 && frame[104].g < 80); // top left corner: red
    CHECK(frame[0].b > 200 && frame[0].r < 60);     // bottom of the right side: blue
    CHECK(frame[60].g > frame[60].r);               // top right: green
    // A front right lock-up flashes the upper half of the right side.
    in.wheel[1] = WHEEL_LOCKING;
    r.render(in, 1200, frame); // (1200 / 60) % 2 == 0: flash on
    CHECK(frame[30].r > 200 && frame[30].g < 60);
}

static void testSpeedTheme()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_SPEED;
    r.settings.alerts = 0;
    Input in = racing(5000, 8000);
    in.speedKmh = 0;
    r.render(in, 1000, frame);
    const int still = brightness(frame[30]);
    in.speedKmh = 250;
    int brightest = 0;
    for (uint32_t t = 1016; t < 3000; t += 16)
    {
        r.render(in, t, frame);
        for (int i = 0; i < 143; ++i) brightest = brightness(frame[i]) > brightest ? brightness(frame[i]) : brightest;
    }
    CHECK(brightest > still + 200);
}

static void testAlerts()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    r.settings.theme = THEME_DEFAULT;
    r.settings.gurgle = 0;
    r.settings.alerts = ALERTS_ALL;
    Input in = racing(7400, 7400);
    in.limiter = true;
    // The limiter flashes the whole strip red, 50 ms on and 50 off.
    r.render(in, 1000, frame);
    bool allRed = true;
    for (int i = 0; i < 143; ++i) allRed = allRed && same(frame[i], RED);
    r.render(in, 1060, frame);
    bool allBlack = true;
    for (int i = 0; i < 143; ++i) allBlack = allBlack && isBlack(frame[i]);
    CHECK(allRed && allBlack);
    // Switched off: the theme stays.
    r.settings.alerts = 0;
    r.render(in, 1200, frame);
    CHECK(same(frame[0], GREEN) && same(frame[142], RED));

    // Best lap: purple pulses for 1.8 s.
    r.settings.alerts = ALERT_BEST_LAP;
    in.limiter = false;
    in.rpm = 1000;
    r.trigger(Event::BestLap, 5000);
    r.render(in, 5200, frame);
    CHECK(same(frame[70], PURPLE));
    r.render(in, 7000, frame);
    CHECK(!same(frame[70], PURPLE));
    // An alert that is off is not drawn.
    r.settings.alerts = 0;
    r.trigger(Event::BestLap, 8000);
    r.render(in, 8200, frame);
    CHECK(!same(frame[70], PURPLE));

    // Sector: the top in the sector's colour.
    r.settings.alerts = ALERT_SECTORS;
    r.trigger(Event::SectorGreen, 9000);
    r.render(in, 9300, frame);
    CHECK(same(frame[71], AHEAD) && !same(frame[10], AHEAD));

    // Final lap: white flashes; finish: chequered flag.
    r.settings.alerts = ALERT_FLAGS;
    r.trigger(Event::FinalLap, 10000);
    r.render(in, 10100, frame);
    CHECK(same(frame[71], WHITE) && same(frame[0], WHITE));
    r.trigger(Event::Finish, 12000);
    r.render(in, 12100, frame);
    int white = 0;
    for (int i = 0; i < 143; ++i) white += same(frame[i], WHITE);
    CHECK(white > 50 && white < 93);
    // The flag also shows when the game has already left the track.
    in.racing = false;
    r.render(in, 13000, frame);
    white = 0;
    for (int i = 0; i < 143; ++i) white += same(frame[i], WHITE);
    CHECK(white > 50);
    in.racing = true;

    // Fuel: amber on the lower half of the sides.
    r.settings.alerts = ALERT_FUEL;
    r.trigger(Event::LowFuel, 20000);
    r.render(in, 20200, frame);
    CHECK(same(frame[5], AMBER) && same(frame[137], AMBER) && !same(frame[71], AMBER));

    // Wheels: a rear left lock-up turns the lower left side red.
    r.settings.alerts = ALERT_GRIP;
    in.wheel[2] = WHEEL_LOCKING;
    r.render(in, 21000, frame);
    CHECK(frame[140].r > 200 && frame[140].g < 40);
    CHECK(frame[110].r < 200 || frame[110].g > 40); // upper left: no
    in.wheel[2] = WHEEL_GRIP;

    // Braking point 120 m ahead: the upper part of the sides, white.
    r.settings.alerts = ALERT_BRAKING;
    in.speedKmh = 220;
    in.hasBrakePoint = true;
    in.brakePointM = 120;
    r.render(in, 22000, frame);
    CHECK(brightness(frame[37]) > 500 && same(frame[10], GREEN)); // the theme below 80% of the height
    in.brakePointM = 30; // red, longer
    r.render(in, 22016, frame);
    CHECK(frame[37].r > 200 && frame[37].g < 40 && frame[20].r > 200);
    in.brake = 0.8f; // already braking: gone
    r.render(in, 22032, frame);
    CHECK(!(frame[37].r > 200 && frame[37].g < 40));
    in.brake = 0;
    in.hasBrakePoint = false;

    // Suggested gear lower than the gear in use: cyan top corners.
    r.settings.alerts = ALERT_GEAR;
    in.gear = 5;
    in.suggestedGear = 3;
    r.render(in, 23040, frame); // (23040 / 90) % 2 == 0
    CHECK(same(frame[39], CYAN) && same(frame[103], CYAN) && !same(frame[71], CYAN));
}

static void testLayoutTest()
{
    Renderer r;
    Layout l;
    l.right = 30; l.top = 60; l.left = 35;
    r.setLayout(l);
    r.startLayoutTest(1000, 5000);
    Input in = racing();
    r.render(in, 1300, frame); // the first LED is off in this phase
    CHECK(same(frame[1], Rgb(255, 0, 0)) && same(frame[29], Rgb(255, 0, 0)));
    CHECK(same(frame[30], Rgb(0, 255, 0)) && same(frame[89], Rgb(0, 255, 0)));
    CHECK(same(frame[90], Rgb(0, 0, 255)) && same(frame[124], Rgb(0, 0, 255)));
    r.render(in, 1100, frame);
    CHECK(same(frame[0], WHITE));
    r.render(in, 6500, frame);
    CHECK(!r.layoutTestActive(6500));
}

static void testIdle()
{
    Renderer r;
    r.setLayout(Layout::forCount(143));
    Input in; // not racing
    r.settings.idleColor = Rgb(0, 100, 255);
    r.settings.idleMode = IDLE_OFF;
    r.render(in, 1000, frame);
    CHECK(isBlack(frame[0]) && isBlack(frame[100]));
    r.settings.idleMode = IDLE_SOLID;
    r.render(in, 1000, frame);
    CHECK(same(frame[50], Rgb(0, 100, 255)));
    r.settings.idleMode = IDLE_TV;
    r.render(in, 1000, frame);
    CHECK(same(frame[50], WHITE));
    // Breathing: FastLED beatsin8(20, 20, 255), between 20 and 255.
    r.settings.idleMode = IDLE_BREATHING;
    int lo = 999, hi = 0;
    for (uint32_t t = 0; t < 3000; t += 20)
    {
        r.render(in, t, frame);
        lo = frame[0].b < lo ? frame[0].b : lo;
        hi = frame[0].b > hi ? frame[0].b : hi;
    }
    CHECK(lo < 30 && hi > 240);
    // Rainbow: neighbouring LEDs differ a little.
    r.settings.idleMode = IDLE_RAINBOW;
    r.render(in, 5000, frame);
    CHECK(!same(frame[0], frame[100]));
    // Fire: something burns at the bottom after a while, the top centre stays darker.
    r.settings.idleMode = IDLE_FIRE;
    long bottom = 0, middle = 0;
    for (uint32_t t = 0; t < 4000; t += 16)
    {
        r.render(in, t, frame);
        if (t > 2000) { bottom += brightness(frame[3]) + brightness(frame[139]); middle += brightness(frame[71]); }
    }
    CHECK(bottom > 0 && bottom > middle);
    // Wave and aurora: lit, never all black.
    r.settings.idleMode = IDLE_WAVE;
    r.render(in, 1300, frame);
    int lit = 0;
    for (int i = 0; i < 143; ++i) lit += brightness(frame[i]) > 0;
    CHECK(lit == 143);
    r.settings.idleMode = IDLE_AURORA;
    r.render(in, 1300, frame);
    lit = 0;
    for (int i = 0; i < 143; ++i) lit += brightness(frame[i]) > 0;
    CHECK(lit == 143);
}

// Every theme and animation on odd layouts, under the address sanitizer.
static void testOddLayouts()
{
    static const int sides[][3] = {{0, 1, 0}, {1, 0, 0}, {0, 0, 1}, {1, 1, 1}, {0, 7, 0}, {3, 0, 3},
                                   {60, 80, 60}, {0, 200, 0}, {100, 0, 100}, {2, 3, 0}};
    for (const auto &s : sides)
        for (int mirror = 0; mirror < 2; ++mirror)
        {
            Renderer r;
            Layout l;
            l.right = s[0]; l.top = s[1]; l.left = s[2]; l.startLeft = mirror != 0;
            r.setLayout(l);
            CHECK(r.count() == l.count());
            r.settings.alerts = ALERTS_ALL;
            r.settings.gurgle = 255;
            r.trigger(Event::BestLap, 0);
            r.trigger(Event::Finish, 0);
            r.startLayoutTest(0, 100);
            Input in = racing(7000, 7400);
            in.hasDelta = true;
            in.deltaS = 0.3f;
            in.lateralG = 0.5f;
            in.hasBrakePoint = true;
            in.brakePointM = 70;
            in.wheel[0] = WHEEL_SPINNING;
            in.tyreTemp[0] = 90;
            uint32_t t = 0;
            for (int theme = 0; theme < THEME_COUNT; ++theme)
            {
                r.settings.theme = static_cast<uint8_t>(theme);
                for (int k = 0; k < 20; ++k, t += 16) r.render(in, t, frame);
            }
            in.racing = false;
            for (int mode = 0; mode < IDLE_COUNT; ++mode)
            {
                r.settings.idleMode = static_cast<uint8_t>(mode);
                for (int k = 0; k < 20; ++k, t += 16) r.render(in, t, frame);
            }
        }
    // An invalid layout falls back to a valid one.
    Renderer r;
    Layout l;
    l.right = 150; l.top = 150; l.left = 150;
    r.setLayout(l);
    CHECK(r.count() <= MAX_LEDS);
}

int main()
{
    testColourCode();
    testLayout();
    testGeometry();
    testClassicThemesUnchanged();
    testGurgle();
    testDeltaTheme();
    testPedalsTheme();
    testGForceTheme();
    testTyresTheme();
    testSpeedTheme();
    testAlerts();
    testLayoutTest();
    testIdle();
    testOddLayouts();
    if (failures) { printf("%d check(s) failed\n", failures); return 1; }
    printf("led effects: all checks passed\n");
    return 0;
}
