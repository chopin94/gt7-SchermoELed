// Host stub: the screenshots show a connected dashboard with a fixed address.
#pragma once
#include <Arduino.h>

enum wl_status_t { WL_IDLE_STATUS = 0, WL_CONNECTED = 3, WL_DISCONNECTED = 6 };
enum wifi_mode_t { WIFI_OFF = 0, WIFI_STA = 1, WIFI_AP = 2, WIFI_AP_STA = 3 };

class IPAddress {
public:
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : v_{a, b, c, d} {}
    uint8_t operator[](int i) const { return v_[i]; }
    uint8_t &operator[](int i) { return v_[i]; }
    operator uint32_t() const { return v_[0] | v_[1] << 8 | v_[2] << 16 | (uint32_t)v_[3] << 24; }
    bool operator==(const IPAddress &o) const { return memcmp(v_, o.v_, 4) == 0; }
    bool operator!=(const IPAddress &o) const { return !(*this == o); }
    String toString() const
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", v_[0], v_[1], v_[2], v_[3]);
        return String(buf);
    }
private:
    uint8_t v_[4] = {0, 0, 0, 0};
};

struct HostWiFi {
    wl_status_t connectedStatus = WL_CONNECTED;
    wl_status_t status() const { return connectedStatus; }
    IPAddress localIP() const { return IPAddress(192, 168, 1, 42); }
    IPAddress subnetMask() const { return IPAddress(255, 255, 255, 0); }
    IPAddress broadcastIP() const { return IPAddress(192, 168, 1, 255); }
    String SSID() const { return "Casa"; }
    wifi_mode_t getMode() const { return WIFI_STA; }
    bool mode(wifi_mode_t) { return true; }
    void begin() {}
    void setAutoReconnect(bool) {}
    void softAPdisconnect(bool) {}
};
extern HostWiFi WiFi;

inline void configTime(long, int, const char *, const char * = nullptr) {}
inline void configTzTime(const char *, const char *, const char * = nullptr) {}
