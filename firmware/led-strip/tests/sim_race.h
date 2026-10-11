// A simulated race for the tests and the previews of the LED strip: the
// driver of the screen's screenshots (tools/screenshots/track_sim.h) laps a
// 4 km circuit at 60 Hz and every step becomes a GT7 packet (Race::Packet).
#pragma once
#include "../../../tools/screenshots/track_sim.h"
#include "../src/RaceTelemetry.h"

struct SimRace
{
    TrackSim track;
    float s = -900;          // metres from the line: the race starts on the out-lap
    int32_t packetId = 100000;
    int lapCount = 0, totalLaps = 3;
    double time = 0, lapStart = 0;
    bool lapStarted = false;
    int32_t lastLapMs = -1, bestLapMs = -1;
    float speed = 0, accel = 0;
    float fuel = 60;
    uint32_t nowMs = 5000;   // millis() of the strip
    float pace = 1.0f;       // share of the speed profile
    float heading = 0;

    SimRace()
    {
        track.build({{0, 0}, {420, 0}, {600, 30}, {660, 170}, {560, 300}, {600, 430}, {770, 480},
                     {830, 620}, {720, 730}, {560, 650}, {430, 520}, {270, 560}, {130, 700},
                     {-60, 650}, {-150, 470}, {-90, 300}, {-170, 170}, {-120, 40}});
        track.profile(80.0f, 13.5f, 13.0f, 8.5f);
    }

    float lapFraction() const
    {
        const float f = fmodf(s, track.length) / track.length;
        return f < 0 ? f + 1 : f;
    }

    Race::Packet packet() const
    {
        Race::Packet p;
        p.packetId = packetId;
        p.flags = Race::FLAG_ON_TRACK | Race::FLAG_IN_GEAR;
        const float kmh = speed * 3.6f;
        static const float shift[] = {0, 62, 98, 132, 166, 200, 236, 400};
        int gear = 1;
        while (gear < 7 && kmh > shift[gear]) gear++;
        const float low = shift[gear - 1] * 0.62f, high = shift[gear];
        const float pct = fminf(1.0f, fmaxf(0.3f, 0.5f + 0.5f * (kmh - low) / (high - low)));
        p.rpm = 8000 * pct;
        p.maxAlertRpm = 8000;
        p.minAlertRpm = 7000;
        if (pct >= 0.995f) p.flags |= Race::FLAG_LIMITER;
        p.speed = speed;
        p.gear = static_cast<uint8_t>(gear);
        p.throttle = static_cast<uint8_t>(255 * (accel > 0.3f ? fminf(1.0f, 0.35f + accel / 8.0f) : accel < -1.0f ? 0.0f : 0.25f));
        p.brake = static_cast<uint8_t>(255 * (accel < -1.0f ? fminf(1.0f, -accel / 13.0f) : 0.0f));
        p.throttleOut = p.throttle;
        p.brakeOut = p.brake;
        const TrackSim::Node n = track.at(s < 0 ? s + track.length : s);
        p.position[0] = n.x; p.position[1] = 0; p.position[2] = n.z;
        p.velocity[0] = n.hx * speed; p.velocity[2] = n.hz * speed;
        p.fuelLevel = fuel;
        p.fuelCapacity = 100;
        p.boost = 1.0f + fmaxf(0.0f, pct - 0.4f) * 1.6f;
        p.oilPressure = 4.2f; p.waterTemp = 85; p.oilTemp = 110;
        const float baseTemp[4] = {88, 93, 81, 84};
        for (int w = 0; w < 4; ++w)
        {
            p.tyreTemp[w] = baseTemp[w];
            p.tyreRadius[w] = 0.33f;
            p.wheelRps[w] = speed / 0.33f;
            p.suspHeight[w] = 0.12f;
        }
        p.lapCount = lapCount;
        p.totalLaps = totalLaps;
        p.currentLapMs = lapStarted ? static_cast<int32_t>((time - lapStart) * 1000.0) : 0;
        p.lastLapMs = lastLapMs;
        p.bestLapMs = bestLapMs;
        p.dayMs = 14 * 3600000;
        p.startPosition = -1;
        p.carCode = 3333;
        return p;
    }

    // One packet of 1/60 s; returns it.
    Race::Packet step()
    {
        const float dt = 1.0f / 60.0f;
        const float target = track.speedAt((s < 0 ? s + track.length : s) + speed * 0.25f) * pace;
        const float before = speed;
        if (target > speed) speed = fminf(target, speed + 9.5f * (1.0f - speed / 92.0f) * dt);
        else speed = fmaxf(target, speed - 13.0f * dt);
        accel = (speed - before) / dt;
        const float prev = s;
        s += speed * dt;
        time += dt;
        packetId++;
        nowMs += 17;
        fuel -= 0.0009f;
        if (floorf(prev / track.length) != floorf(s / track.length))
        {
            const double crossing = time - fmodf(s, track.length) / fmaxf(speed, 1.0f);
            if (lapStarted)
            {
                lastLapMs = static_cast<int32_t>((crossing - lapStart) * 1000.0);
                if (bestLapMs < 0 || lastLapMs < bestLapMs) bestLapMs = lastLapMs;
            }
            lapStart = crossing;
            lapStarted = true;
            lapCount++;
        }
        return packet();
    }
};
