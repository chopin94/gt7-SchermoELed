// Minimal Arduino API for rendering the dashboard on a PC (screenshots only).
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <algorithm>
#include <cmath>
#include <string>

using std::isfinite;
using std::isnan;
using std::max;
using std::min;

typedef uint8_t byte;
typedef bool boolean;

#define PROGMEM
#define F(s) (s)
#define pgm_read_byte(addr) (*(const uint8_t *)(addr))
#define pgm_read_word(addr) (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#include <time.h>
#define HEX 16
#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI 6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define DEC 10

// Virtual clock: the renderer moves it forward to drive animations.
extern uint32_t g_hostMillis;
inline uint32_t millis() { return g_hostMillis; }
inline uint32_t micros() { return g_hostMillis * 1000u; }
inline void delay(uint32_t ms) { g_hostMillis += ms; }
inline void yield() {}
inline long random(long howbig) { return howbig > 0 ? rand() % howbig : 0; }
inline long random(long lo, long hi) { return hi > lo ? lo + rand() % (hi - lo) : lo; }

template <typename T, typename L, typename H>
inline T constrain(T x, L lo, H hi) { return x < (T)lo ? (T)lo : x > (T)hi ? (T)hi : x; }
inline long map(long x, long in_min, long in_max, long out_min, long out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

class String {
public:
    String() {}
    String(const char *s) : s_(s ? s : "") {}
    String(const std::string &s) : s_(s) {}
    String(char c) : s_(1, c) {}
    String(int v, int base = 10) { fromLong(v, base); }
    String(unsigned v, int base = 10) { fromULong(v, base); }
    String(long v, int base = 10) { fromLong(v, base); }
    String(unsigned long v, int base = 10) { fromULong(v, base); }
    String(float v, int decimals = 2) { fromDouble(v, decimals); }
    String(double v, int decimals = 2) { fromDouble(v, decimals); }

    operator const char *() const { return s_.c_str(); }
    const char *c_str() const { return s_.c_str(); }
    unsigned length() const { return (unsigned)s_.size(); }
    bool isEmpty() const { return s_.empty(); }
    char charAt(unsigned i) const { return i < s_.size() ? s_[i] : 0; }
    char operator[](unsigned i) const { return charAt(i); }

    String &operator+=(const String &o) { s_ += o.s_; return *this; }
    String &operator+=(const char *o) { s_ += o ? o : ""; return *this; }
    String &operator+=(char c) { s_ += c; return *this; }
    String &operator+=(int v) { return *this += String(v); }
    friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }
    friend String operator+(const String &a, const char *b) { return String(a.s_ + (b ? b : "")); }
    friend String operator+(const char *a, const String &b) { return String(std::string(a ? a : "") + b.s_); }
    friend String operator+(const String &a, char b) { return String(a.s_ + b); }
    friend String operator+(const String &a, int b) { return a + String(b); }
    friend String operator+(const String &a, unsigned b) { return a + String(b); }
    friend String operator+(const String &a, long b) { return a + String(b); }
    friend String operator+(const String &a, unsigned long b) { return a + String(b); }
    friend String operator+(const String &a, float b) { return a + String(b); }
    friend String operator+(const String &a, double b) { return a + String(b); }

    bool operator==(const String &o) const { return s_ == o.s_; }
    bool operator==(const char *o) const { return s_ == (o ? o : ""); }
    bool operator!=(const String &o) const { return s_ != o.s_; }
    bool operator!=(const char *o) const { return s_ != (o ? o : ""); }
    bool operator<(const String &o) const { return s_ < o.s_; }
    bool equals(const String &o) const { return s_ == o.s_; }

    int indexOf(char c, unsigned from = 0) const { return find(s_.find(c, from)); }
    int indexOf(const String &t, unsigned from = 0) const { return find(s_.find(t.s_, from)); }
    int lastIndexOf(char c) const { return find(s_.rfind(c)); }
    int lastIndexOf(char c, unsigned from) const { return find(s_.rfind(c, from)); }
    int lastIndexOf(const String &t) const { return find(s_.rfind(t.s_)); }
    String substring(unsigned from) const { return from >= s_.size() ? String() : String(s_.substr(from)); }
    String substring(unsigned from, unsigned to) const
    {
        if (to > s_.size()) to = (unsigned)s_.size();
        if (from >= to) return String();
        return String(s_.substr(from, to - from));
    }
    bool startsWith(const String &p) const { return s_.compare(0, p.s_.size(), p.s_) == 0; }
    bool endsWith(const String &p) const
    {
        return s_.size() >= p.s_.size() && s_.compare(s_.size() - p.s_.size(), p.s_.size(), p.s_) == 0;
    }
    long toInt() const { return strtol(s_.c_str(), nullptr, 10); }
    float toFloat() const { return strtof(s_.c_str(), nullptr); }
    void trim()
    {
        const auto b = s_.find_first_not_of(" \t\r\n");
        const auto e = s_.find_last_not_of(" \t\r\n");
        s_ = b == std::string::npos ? std::string() : s_.substr(b, e - b + 1);
    }
    void replace(const String &from, const String &to)
    {
        if (from.s_.empty()) return;
        for (size_t p = 0; (p = s_.find(from.s_, p)) != std::string::npos; p += to.s_.size())
            s_.replace(p, from.s_.size(), to.s_);
    }
    void toUpperCase() { for (auto &c : s_) c = (char)toupper((unsigned char)c); }
    void reserve(unsigned) {}

private:
    std::string s_;
    static int find(size_t p) { return p == std::string::npos ? -1 : (int)p; }
    void fromLong(long v, int base)
    {
        if (base == 10) { s_ = std::to_string(v); return; }
        fromULong((unsigned long)v, base);
    }
    void fromULong(unsigned long v, int base)
    {
        char buf[40];
        snprintf(buf, sizeof(buf), base == 16 ? "%lx" : "%lu", v);
        s_ = buf;
    }
    void fromDouble(double v, int decimals)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%.*f", decimals, v);
        s_ = buf;
    }
};

// Fixed wall clock (14:35) so screenshots are reproducible.
inline bool getLocalTime(struct tm *info, uint32_t = 5000)
{
    memset(info, 0, sizeof(*info));
    info->tm_hour = 14; info->tm_min = 35; info->tm_year = 126; info->tm_mon = 9; info->tm_mday = 6;
    return true;
}

struct HostSerial {
    void begin(unsigned long) {}
    template <typename T> void print(const T &) {}
    template <typename T> void println(const T &) {}
    void println() {}
    template <typename... A> void printf(const char *, A...) {}
};
extern HostSerial Serial;
