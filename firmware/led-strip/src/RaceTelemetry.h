#pragma once
#include <math.h>
#include <stdint.h>
#include <string.h>
#include "../../dashboard/src/LapAnalysis.h"
#include "../../dashboard/src/GripAnalysis.h"
#include "../../dashboard/src/TrackStore.h"
#include "LedEffects.h"
#include "JsonOut.h"

// The race as the LED strip sees it. Every GT7 packet goes through the same
// lap and grip analysis as the screen (firmware/dashboard/src): live delta,
// sectors, braking points, wheels. The result drives the effects of the strip
// and the telemetry of its web page. Plain C++11, tested on the PC
// (tests/race_telemetry.cpp).
namespace Race
{
// GT7 flags (GT7UDPParser.h, SimulatorFlags).
enum : uint16_t
{
    FLAG_ON_TRACK = 1 << 0,
    FLAG_PAUSED = 1 << 1,
    FLAG_LOADING = 1 << 2,
    FLAG_IN_GEAR = 1 << 3,
    FLAG_TURBO = 1 << 4,
    FLAG_LIMITER = 1 << 5,
    FLAG_HANDBRAKE = 1 << 6,
    FLAG_LIGHTS = 1 << 7,
    FLAG_HIGH_BEAM = 1 << 8,
    FLAG_LOW_BEAM = 1 << 9,
    FLAG_ASM = 1 << 10,
    FLAG_TCS = 1 << 11,
};

// The fields of a GT7 packet the strip uses (copied in main.cpp).
struct Packet
{
    int32_t packetId = 0;
    uint16_t flags = 0;
    float rpm = 0;
    float minAlertRpm = 0, maxAlertRpm = 0;
    float speed = 0;                       // m/s
    uint8_t gear = 0;                      // 0 reverse, 15 neutral
    uint8_t suggestedGear = 15;            // 15: none
    uint8_t throttle = 0, brake = 0;       // 0-255, the driver's pedals
    uint8_t throttleOut = 0, brakeOut = 0; // after traction control and ABS
    float clutch = 0;                      // 0-1
    float position[3] = {0, 0, 0};
    float velocity[3] = {0, 0, 0};
    float fuelLevel = 0, fuelCapacity = 0;
    float boost = NAN;                     // GT7 sends it offset by +1
    float oilPressure = NAN, waterTemp = NAN, oilTemp = NAN;
    float tyreTemp[4] = {NAN, NAN, NAN, NAN};
    float wheelRps[4] = {0, 0, 0, 0};
    float tyreRadius[4] = {0, 0, 0, 0};
    float suspHeight[4] = {NAN, NAN, NAN, NAN};
    int lapCount = 0, totalLaps = 0;
    int32_t bestLapMs = -1, lastLapMs = -1, currentLapMs = -1;
    int32_t dayMs = -1;                    // time of day on the track
    int startPosition = -1, cars = -1;
    int32_t carCode = 0;
};

// Fuel used per lap, as the screen estimates it (GT7FuelEstimator).
class FuelEstimator
{
public:
    void reset() { lapStart = last = -1; lastLap = -1; perLap = 0; samples = 0; remaining = -1; }
    void update(float level, int lap)
    {
        if (lap < 0 || level < 0) return;
        if (lastLap < 0 || lap < lastLap) { reset(); lastLap = lap; lapStart = last = level; return; }
        if (level > last + 0.5f) lapStart = level; // refuelled
        if (lap > lastLap)
        {
            const float used = lapStart - level;
            // The 0 to 1 switch at the start is not a lap.
            if (lastLap >= 1 && lap == lastLap + 1 && used >= 0.2f && used <= 50.0f)
            {
                perLap = samples == 0 ? used : perLap * 0.7f + used * 0.3f;
                samples++;
            }
            lastLap = lap;
            lapStart = level;
        }
        last = level;
        remaining = samples > 0 && perLap >= 0.2f ? level / perLap : -1;
    }
    float lapsLeft() const { return remaining; }
    float usedPerLap() const { return samples > 0 ? perLap : -1; }

private:
    float lapStart = -1, last = -1, perLap = 0, remaining = -1;
    int lastLap = -1, samples = 0;
};

class Telemetry
{
public:
    LapAnalysis::Analyzer lap;
    GripAnalysis::Analyzer grip;
    // The analyses cost time on the ESP32-S2 (no floating point unit): main.cpp
    // switches them off when no theme or alert uses them.
    bool lapAnalysisOn = true, gripAnalysisOn = true;

    Telemetry() { fuel.reset(); }

    void setTrackStore(TrackStore::Store *store) { trackStore = store; }

    // Everything about the session, also the saved data of this circuit.
    void resetSession()
    {
        if (trackStore && lap.trackKey() != 0) trackStore->eraseTrack(lap.trackKey());
        lap.reset();
        grip.reset();
        fuel.reset();
        referenceTriedKey = 0;
        for (int i = 0; i < 3; ++i) sectorSeen[i] = LapAnalysis::SectorState::None;
        sectorLaps = 0;
    }

    // One new packet (its packetId differs from the last one).
    void update(const Packet &p, uint32_t nowMs)
    {
        // Packets per second, measured over one second.
        rateCount++;
        if (nowMs - rateStartMs >= 1000)
        {
            rate = rateCount * 1000.0f / (nowMs - rateStartMs);
            rateCount = 0;
            rateStartMs = nowMs;
        }
        last = p;
        hasPacket = true;
        lastPacketMs = nowMs;

        const bool driving = onTrack();
        // Another car on the same circuit: times start over, the map stays.
        if (p.carCode != 0 && carCode != 0 && p.carCode != carCode)
        {
            lap.resetTiming();
            grip.resetCar();
            fuel.reset();
        }
        if (p.carCode != 0) carCode = p.carCode; // GT7 sends 0 now and then

        const bool ev = isfinite(p.fuelCapacity) && p.fuelCapacity >= 0.0f && p.fuelCapacity < 0.1f;
        const bool iceFuel = !ev && isfinite(p.fuelCapacity) && isfinite(p.fuelLevel) && p.fuelLevel >= 0.0f;
        if (iceFuel) fuel.update(p.fuelLevel, p.lapCount > 0 ? p.lapCount : 0);
        updateAbs(p, nowMs);

        LapAnalysis::Sample s;
        // GT7 sends one packet per frame at 60 Hz: the packet id is the clock.
        s.timeMs = clock();
        s.speed = isfinite(p.speed) ? p.speed : 0.0f;
        s.hasPosition = isfinite(p.position[0]) && isfinite(p.position[2]);
        s.x = p.position[0];
        s.y = isfinite(p.position[1]) ? p.position[1] : 0.0f;
        s.z = p.position[2];
        s.hasVelocity = isfinite(p.velocity[0]) && isfinite(p.velocity[2]);
        s.vx = p.velocity[0];
        s.vz = p.velocity[2];
        s.lapCount = p.lapCount;
        s.totalLaps = p.totalLaps;
        s.lapTimeMs = p.currentLapMs >= 0 ? p.currentLapMs : -1;
        s.lastLapMs = p.lastLapMs;
        s.driving = driving;
        s.throttle = p.throttle / 255.0f;
        s.brake = p.brake / 255.0f;
        s.fuelFraction = iceFuel && p.fuelCapacity > 0.0f ? p.fuelLevel / p.fuelCapacity : NAN;
        s.fuelLaps = fuel.lapsLeft() >= 0.0f ? fuel.lapsLeft() : NAN;
        if (lapAnalysisOn) lap.update(s);

        GripAnalysis::Sample g;
        g.timeMs = s.timeMs;
        g.speed = s.speed;
        g.driving = driving;
        g.hasWheels = true;
        for (int i = 0; i < GripAnalysis::WHEELS; ++i)
        {
            g.wheelRps[i] = p.wheelRps[i];
            g.tyreRadius[i] = p.tyreRadius[i];
            g.tyreTemp[i] = p.tyreTemp[i];
            g.suspension[i] = p.suspHeight[i];
        }
        g.throttle = s.throttle;
        g.brake = s.brake;
        g.lateralG = lap.lateralG();
        g.longitudinalG = lap.longitudinalG();
        g.lapCount = p.lapCount;
        if (gripAnalysisOn) grip.update(g);

        if (lapAnalysisOn)
        {
            serviceTrackStore(nowMs);
            detectEvents(p);
        }
    }

    // Un buco di un paio di secondi (Wi-Fi) non spegne il tema di gara.
    bool live(uint32_t nowMs) const { return hasPacket && nowMs - lastPacketMs <= 2500; }
    // On track, not paused: the race themes are on. The same rule as before.
    bool racing(uint32_t nowMs) const
    {
        return live(nowMs) && last.packetId != 0 && last.maxAlertRpm > 0 && onTrack();
    }
    uint32_t lastPacketTime() const { return lastPacketMs; }
    bool everReceived() const { return hasPacket; }
    const Packet &packet() const { return last; }
    float packetRate(uint32_t nowMs) const { return live(nowMs) ? rate : 0.0f; }
    float fuelLapsLeft() const { return fuel.lapsLeft(); }
    float fuelPerLap() const { return fuel.usedPerLap(); }

    // What the effects need.
    void fillInput(LedFx::Input &in, uint32_t nowMs) const
    {
        const Packet &p = last;
        in = LedFx::Input();
        in.racing = racing(nowMs);
        in.rpm = p.rpm;
        in.maxRpm = p.maxAlertRpm;
        in.alertRpm = p.minAlertRpm;
        in.limiter = (p.flags & FLAG_LIMITER) != 0;
        in.speedKmh = p.speed * 3.6f;
        in.throttle = p.throttle / 255.0f;
        in.brake = p.brake / 255.0f;
        in.gear = p.gear == 15 ? 0 : p.gear;
        in.suggestedGear = p.suggestedGear >= 1 && p.suggestedGear <= 9 ? p.suggestedGear : -1;
        in.hasDelta = lapAnalysisOn && lap.hasLiveDelta();
        in.deltaS = lap.liveDeltaMs() / 1000.0f;
        in.lateralG = lap.lateralG();
        in.longitudinalG = lap.longitudinalG();
        for (int w = 0; w < 4; ++w)
        {
            in.tyreTemp[w] = p.tyreTemp[w];
            in.wheel[w] = gripAnalysisOn ? static_cast<uint8_t>(grip.state(w)) : 0;
        }
        in.timeOfDayH = timeOfDay();
        float meters = 0, kmh = 0;
        in.hasBrakePoint = lapAnalysisOn && lap.nextBrakePoint(meters, kmh);
        in.brakePointM = meters;
    }

    // Events for the effects: best lap, sectors, fuel, final lap, finish.
    bool takeEvent(LedFx::Event &event)
    {
        if (eventCount == 0) return false;
        event = events[eventHead];
        eventHead = (eventHead + 1) % EVENT_QUEUE;
        eventCount--;
        return true;
    }

    // ---- JSON for the web page ----
    // One frame of live telemetry (about 1 KB), sent 20 times a second.
    void writeFrame(JsonOut &j, uint32_t nowMs) const
    {
        const Packet &p = last;
        j.beginObject();
        j.field("pt", static_cast<long long>(clock()));
        j.flag("live", live(nowMs));
        j.flag("race", racing(nowMs));
        j.field("f", static_cast<long long>(p.flags));
        j.field("hz", packetRate(nowMs), 1);
        j.field("rpm", p.rpm, 0);
        j.field("mx", p.maxAlertRpm, 0);
        j.field("al", p.minAlertRpm, 0);
        j.field("kmh", p.speed * 3.6f, 1);
        j.field("g", static_cast<long long>(p.gear));
        j.field("sg", static_cast<long long>(p.suggestedGear));
        j.field("thr", static_cast<long long>(p.throttle * 100 / 255));
        j.field("brk", static_cast<long long>(p.brake * 100 / 255));
        j.field("thro", static_cast<long long>(p.throttleOut * 100 / 255));
        j.field("brko", static_cast<long long>(p.brakeOut * 100 / 255));
        j.field("clu", p.clutch * 100.0f, 0);
        j.flag("abs", absActive);
        j.field("fuel", p.fuelLevel, 1);
        j.field("cap", p.fuelCapacity, 1);
        j.key("fl"); positiveOrNull(j, fuel.lapsLeft(), 1);
        j.key("fpl"); positiveOrNull(j, fuel.usedPerLap(), 2);
        j.field("boost", (p.flags & FLAG_TURBO) ? p.boost - 1.0f : NAN, 2);
        j.field("oil", p.oilPressure, 1);
        j.field("wt", p.waterTemp, 0);
        j.field("ot", p.oilTemp, 0);
        j.key("tt").beginArray();
        for (int w = 0; w < 4; ++w) j.number(p.tyreTemp[w], 0);
        j.endArray();
        j.key("sl").beginArray();
        for (int w = 0; w < 4; ++w) j.number(grip.slip(w) * 100.0f, 1);
        j.endArray();
        j.key("ws").beginArray();
        for (int w = 0; w < 4; ++w) j.integer(static_cast<int>(grip.state(w)));
        j.endArray();
        j.key("sus").beginArray();
        for (int w = 0; w < 4; ++w) j.number(grip.suspensionLoad(w), 2);
        j.endArray();
        j.field("glat", lap.lateralG(), 2);
        j.field("glon", lap.longitudinalG(), 2);
        j.field("gu", grip.gripUsage(), 2);
        j.key("lk").beginArray()
            .integer(grip.lockEvents()).integer(grip.spinEvents())
            .integer(grip.lastLapLockEvents()).integer(grip.lastLapSpinEvents())
            .endArray();
        j.field("lap", static_cast<long long>(p.lapCount));
        j.field("tl", static_cast<long long>(p.totalLaps));
        j.field("cur", static_cast<long long>(p.currentLapMs));
        j.field("last", static_cast<long long>(p.lastLapMs));
        j.field("best", static_cast<long long>(p.bestLapMs));
        j.key("d");
        if (lap.hasLiveDelta()) j.integer(lap.liveDeltaMs());
        else j.null();
        j.field("ref", static_cast<long long>(lap.referenceLapMs()));
        j.field("sec", static_cast<long long>(lap.currentSector()));
        j.key("ss").beginArray();
        for (int i = 0; i < 3; ++i) j.integer(static_cast<int>(lap.sectorState(i)));
        j.endArray();
        j.key("sm").beginArray();
        for (int i = 0; i < 3; ++i) j.integer(lap.sectorMs(i));
        j.endArray();
        j.key("ls").beginArray();
        for (int i = 0; i < 3; ++i) j.integer(static_cast<int>(lap.lastLapSectorState(i)));
        j.endArray();
        j.key("bs").beginArray();
        for (int i = 0; i < 3; ++i) j.integer(lap.bestSectorMs(i));
        j.endArray();
        float x = NAN, z = NAN;
        lap.carPosition(x, z);
        j.field("x", x, 1);
        j.field("z", z, 1);
        j.field("prog", lap.lapProgress(), 4);
        j.field("pos", static_cast<long long>(p.startPosition));
        j.field("cars", static_cast<long long>(p.cars));
        float meters = 0, kmh = 0;
        if (lap.nextBrakePoint(meters, kmh))
        {
            j.field("nb", meters, 0);
            j.field("nbk", kmh, 0);
            j.field("nbi", static_cast<long long>(lap.nextBrakeIndex()));
        }
        float bd = 0, be = 0, br = 0;
        if (lap.lastBrakeResult(bd, be, br))
        {
            j.key("lb").beginArray().number(bd, 1).number(be, 0).number(br, 0).integer(lap.lastBrakeIndex()).endArray();
        }
        j.field("tod", timeOfDay(), 2);
        j.field("car", static_cast<long long>(carCode));
        j.key("rev").beginArray()
            .integer(lap.sessionRevision()).integer(lap.mapRevision())
            .integer(lap.referenceBrakeRevision()).integer(lap.brakeRevision())
            .integer(lap.launchRevision()).integer(static_cast<long long>(lap.trackKey()))
            .endArray();
        j.endObject();
    }

    // The session: laps, map, braking points, acceleration runs (a few KB),
    // asked for by the page when the revisions in the frames change.
    void writeSession(JsonOut &j) const
    {
        j.beginObject();
        j.field("completed", static_cast<long long>(lap.lapsCompleted()));
        j.field("best", static_cast<long long>(lap.bestLapMs()));
        j.field("avg", static_cast<long long>(lap.averageLapMs()));
        j.field("ref", static_cast<long long>(lap.referenceLapMs()));
        j.field("top", lap.topSpeedKmh(), 1);
        long long ideal = 0;
        for (int i = 0; i < 3; ++i) ideal = lap.bestSectorMs(i) > 0 && ideal >= 0 ? ideal + lap.bestSectorMs(i) : -1;
        j.field("ideal", ideal);
        j.key("bs").beginArray();
        for (int i = 0; i < 3; ++i) j.integer(lap.bestSectorMs(i));
        j.endArray();
        j.key("laps").beginArray();
        for (int age = 0; age < lap.storedLaps(); ++age)
        {
            const LapAnalysis::LapRecord &r = lap.lapRecord(age);
            j.beginObject();
            j.field("n", static_cast<long long>(lap.lapsCompleted() - age));
            j.field("t", static_cast<long long>(r.lapMs));
            j.key("s").beginArray().integer(r.sectorMs[0]).integer(r.sectorMs[1]).integer(r.sectorMs[2]).endArray();
            j.field("v", r.topSpeedKmh, 1);
            j.endObject();
        }
        j.endArray();
        j.key("peaks").beginArray()
            .number(lap.peakLateralG(), 2).number(lap.peakBrakingG(), 2).number(lap.peakAccelerationG(), 2)
            .endArray();

        j.key("map").beginObject();
        j.field("state", static_cast<long long>(lap.mapState()));
        j.field("key", static_cast<long long>(lap.trackKey()));
        float minX, minZ, maxX, maxZ;
        if (lap.mapBounds(minX, minZ, maxX, maxZ))
        {
            j.key("b").beginArray().number(minX, 0).number(minZ, 0).number(maxX, 0).number(maxZ, 0).endArray();
        }
        j.key("p").beginArray();
        const int points = lap.mapPointCount();
        for (int i = 0; i < points; ++i)
        {
            float x, z;
            lap.mapPoint(i, x, z);
            j.beginArray().number(x, 0).number(z, 0).endArray();
        }
        j.endArray();
        j.endObject();

        j.key("brakes").beginArray();
        for (int i = 0; i < lap.referenceBrakeZoneCount(); ++i)
        {
            LapAnalysis::BrakeZone zone;
            if (!lap.referenceBrakeZone(i, zone)) continue;
            j.beginArray().number(zone.x, 0).number(zone.z, 0).number(zone.entryKmh, 0)
                .number(lap.brakeDelta(i), 1).number(lap.lastLapBrakeDelta(i), 1).endArray();
        }
        j.endArray();

        j.key("runs").beginObject();
        writeRun(j, "last", lap.currentRun());
        writeRun(j, "best", lap.bestRuns());
        j.endObject();
        j.endObject();
    }

private:
    static constexpr int EVENT_QUEUE = 8;

    Packet last;
    bool hasPacket = false;
    uint32_t lastPacketMs = 0;
    float rate = 0;
    int rateCount = 0;
    uint32_t rateStartMs = 0;
    int32_t carCode = 0;
    FuelEstimator fuel;

    // ABS at work: a wheel much slower than the car under braking (GT7AbsEstimator).
    bool absActive = false;
    uint32_t absTriggerMs = 0;

    TrackStore::Store *trackStore = nullptr;
    uint32_t serviceTrackKey = 0, referenceTriedKey = 0;
    int32_t referenceTriedCar = 0;
    uint32_t trackScanMs = 0;

    LapAnalysis::SectorState sectorSeen[3] = {LapAnalysis::SectorState::None, LapAnalysis::SectorState::None,
                                               LapAnalysis::SectorState::None};
    int sectorLaps = 0;
    int lastLapCount = 0;
    LedFx::Event events[EVENT_QUEUE];
    int eventHead = 0, eventCount = 0;

    bool onTrack() const
    {
        return (last.flags & FLAG_ON_TRACK) && !(last.flags & FLAG_PAUSED) && !(last.flags & FLAG_LOADING);
    }
    uint32_t clock() const { return static_cast<uint32_t>(static_cast<int64_t>(last.packetId) * 50 / 3); }
    float timeOfDay() const
    {
        if (last.dayMs < 0) return NAN;
        return fmodf(last.dayMs / 3600000.0f, 24.0f);
    }
    static void positiveOrNull(JsonOut &j, float v, int decimals)
    {
        if (v >= 0.0f) j.number(v, decimals);
        else j.null();
    }
    static void writeRun(JsonOut &j, const char *name, const LapAnalysis::LaunchRun &run)
    {
        j.key(name).beginArray()
            .number(run.reaction, 3).number(run.t100, 2).number(run.t200, 2).number(run.t100to200, 2)
            .number(run.t400, 2).number(run.v400Kmh, 0).number(run.vmaxKmh, 0)
            .endArray();
    }

    void updateAbs(const Packet &p, uint32_t nowMs)
    {
        const float kmh = p.speed * 3.6f, brake = p.brake * 100.0f / 255.0f;
        if (kmh < 15.0f || brake < 10.0f)
        {
            absActive = nowMs - absTriggerMs < 200;
            return;
        }
        float maxSlip = 0;
        for (int i = 0; i < 4; ++i)
        {
            const float slip = kmh - fabsf(p.wheelRps[i]) * p.tyreRadius[i] * 3.6f;
            if (slip > maxSlip) maxSlip = slip;
        }
        if (maxSlip > 7.0f) absTriggerMs = nowMs;
        absActive = maxSlip > 7.0f || nowMs - absTriggerMs < 200;
    }

    // Saves what the analysis asks to, loads a saved circuit once the car is
    // on it and then the best lap of this car there (as the screen does).
    void serviceTrackStore(uint32_t nowMs)
    {
        if (!trackStore) return;
        trackStore->service(lap, carCode);
        if (lap.trackKey() != serviceTrackKey)
        {
            serviceTrackKey = lap.trackKey();
            referenceTriedKey = 0;
        }
        if (lap.wantsTrack() && nowMs - trackScanMs >= 2000)
        {
            trackScanMs = nowMs;
            trackStore->restoreTrack(lap);
        }
        if (lap.trackKey() != 0 && lap.referenceLapMs() < 0 &&
            (referenceTriedKey != lap.trackKey() || referenceTriedCar != carCode))
        {
            referenceTriedKey = lap.trackKey();
            referenceTriedCar = carCode;
            trackStore->restoreReference(lap, carCode);
        }
    }

    void push(LedFx::Event e)
    {
        if (eventCount == EVENT_QUEUE) { eventHead = (eventHead + 1) % EVENT_QUEUE; eventCount--; }
        events[(eventHead + eventCount) % EVENT_QUEUE] = e;
        eventCount++;
    }
    static bool closed(LapAnalysis::SectorState s)
    {
        return s == LapAnalysis::SectorState::Purple || s == LapAnalysis::SectorState::Green ||
               s == LapAnalysis::SectorState::Yellow;
    }
    void pushSector(LapAnalysis::SectorState s)
    {
        if (s == LapAnalysis::SectorState::Purple) push(LedFx::Event::SectorPurple);
        else if (s == LapAnalysis::SectorState::Green) push(LedFx::Event::SectorGreen);
        else if (s == LapAnalysis::SectorState::Yellow) push(LedFx::Event::SectorYellow);
    }

    void detectEvents(const Packet &p)
    {
        LapAnalysis::Event e;
        while (lap.takeEvent(e))
        {
            if (e.type == LapAnalysis::EventType::BestLap) push(LedFx::Event::BestLap);
            else if (e.type == LapAnalysis::EventType::FinalLap) push(LedFx::Event::FinalLap);
            else if (e.type == LapAnalysis::EventType::LowFuel) push(LedFx::Event::LowFuel);
        }
        // Sectors: the last one closes with the lap, the others during it.
        const int laps = lap.lapsCompleted();
        if (laps != sectorLaps)
        {
            if (laps > sectorLaps) pushSector(lap.lastLapSectorState(LapAnalysis::SECTORS - 1));
            sectorLaps = laps;
            for (int i = 0; i < 3; ++i) sectorSeen[i] = lap.sectorState(i);
        }
        else
        {
            for (int i = 0; i < 3; ++i)
            {
                const LapAnalysis::SectorState s = lap.sectorState(i);
                if (closed(s) && !closed(sectorSeen[i])) pushSector(s);
                sectorSeen[i] = s;
            }
        }
        // Finish: the lap counter goes past the race length.
        if (p.totalLaps > 0 && p.lapCount == p.totalLaps + 1 && lastLapCount == p.totalLaps) push(LedFx::Event::Finish);
        lastLapCount = p.lapCount;
    }
};
} // namespace Race
