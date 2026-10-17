#pragma once
#include <Arduino.h>
#include <stdio.h>
#include <WiFiUdp.h>
#include "F1Decoder.h"

// Listens to F1 25 on UDP 20777 (broadcast or sent to this address) and
// hands over the player's car, one sample per car telemetry packet.
class F1UdpReceiver
{
public:
    void begin() { udp.begin(F1Udp::PORT); }
    void stop() { udp.stop(); }

    // Reads the waiting datagrams until a new sample is complete. Call it
    // again until it returns false: the lap analysis wants every sample.
    bool poll(F1Udp::Car &car)
    {
        for (int budget = 24; budget > 0; --budget)
        {
            const int size = udp.parsePacket();
            if (size <= 0) return false;
            const int length = udp.read(buffer, sizeof(buffer));
            if (length <= 0 || size > static_cast<int>(sizeof(buffer))) continue;
            countDatagram(length);
            if (decoder.feed(buffer, static_cast<size_t>(length), millis(), car))
            {
                lastSampleMs = millis();
                received = true;
                lastOnTrack = car.onTrack;
                lastPaused = car.paused;
                return true;
            }
        }
        return false;
    }

    // F1 data in the last `ms` milliseconds.
    bool live(uint32_t ms = 2000) const { return received && millis() - lastSampleMs < ms; }

    // What arrives on the port, in words, for the waiting screen and the
    // web page: shows at once whether the game is sending and what.
    void describe(char *out, size_t size)
    {
        countDatagram(0);
        const bool recent = datagramsEver && millis() - lastDatagramMs < 3000;
        if (!datagramsEver || !recent)
            snprintf(out, size, "F1 25: nessun dato sulla porta %u", F1Udp::PORT);
        else if (lastFormat < 2024 || lastFormat > 2026)
            snprintf(out, size, "F1 25: formato UDP %u, scegli 2025", lastFormat);
        else if (!live(3000))
            snprintf(out, size, "F1 25: %u pacchetti/s, nessuna auto", static_cast<unsigned>(rate));
        else if (lastPaused)
            snprintf(out, size, "F1 25: %u pacchetti/s, in pausa", static_cast<unsigned>(rate));
        else if (!lastOnTrack)
            snprintf(out, size, "F1 25: %u pacchetti/s, auto ai box", static_cast<unsigned>(rate));
        else
            snprintf(out, size, "F1 25: %u pacchetti/s, in pista", static_cast<unsigned>(rate));
    }

private:
    WiFiUDP udp;
    F1Udp::Decoder decoder;
    uint8_t buffer[F1Udp::MAX_PACKET];
    uint32_t lastSampleMs = 0;
    bool received = false;
    bool lastOnTrack = false, lastPaused = false;
    // Datagrams per second, the packet format of the last one.
    uint32_t lastDatagramMs = 0, windowStartMs = 0, windowCount = 0;
    float rate = 0;
    bool datagramsEver = false;
    uint16_t lastFormat = 0;

    void countDatagram(int length)
    {
        const uint32_t now = millis();
        if (length > 0)
        {
            if (length >= 2) lastFormat = buffer[0] | (buffer[1] << 8);
            lastDatagramMs = now;
            datagramsEver = true;
            windowCount++;
        }
        if (now - windowStartMs >= 1000)
        {
            rate = windowCount * 1000.0f / (now - windowStartMs);
            windowCount = 0;
            windowStartMs = now;
        }
    }
};
