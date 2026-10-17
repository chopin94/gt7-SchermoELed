#pragma once
#include <Arduino.h>
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
            if (decoder.feed(buffer, static_cast<size_t>(length), millis(), car))
            {
                lastSampleMs = millis();
                received = true;
                return true;
            }
        }
        return false;
    }

    // F1 data in the last `ms` milliseconds.
    bool live(uint32_t ms = 2000) const { return received && millis() - lastSampleMs < ms; }

private:
    WiFiUDP udp;
    F1Udp::Decoder decoder;
    uint8_t buffer[F1Udp::MAX_PACKET];
    uint32_t lastSampleMs = 0;
    bool received = false;
};
