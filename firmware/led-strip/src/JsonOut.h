#pragma once
#include <math.h>
#include <stddef.h>
#include <stdint.h>

// A small JSON writer into a fixed buffer, for the telemetry of the web page.
// Numbers that are not finite are written as null; decimals are written
// without printf (the ESP32-S2 formats floats in software, slowly). When the
// buffer is too small the text is cut and ok() turns false.
class JsonOut
{
public:
    JsonOut(char *buffer, size_t capacity) : buf(buffer), cap(capacity)
    {
        if (cap) buf[0] = 0;
    }
    bool ok() const { return !overflow && depth == 0; }
    size_t length() const { return len; }
    const char *c_str() const { return buf; }

    JsonOut &beginObject() { value(); put('{'); open(); return *this; }
    JsonOut &endObject() { close(); put('}'); return *this; }
    JsonOut &beginArray() { value(); put('['); open(); return *this; }
    JsonOut &endArray() { close(); put(']'); return *this; }

    JsonOut &key(const char *name)
    {
        value();
        quoted(name);
        put(':');
        afterKey = true;
        return *this;
    }
    JsonOut &integer(long long v)
    {
        value();
        digits(v);
        return *this;
    }
    // A number with `decimals` digits after the point (0-4), null if not finite.
    JsonOut &number(float v, int decimals = 1)
    {
        value();
        if (decimals < 0) decimals = 0;
        if (decimals > 4) decimals = 4;
        static const long scales[] = {1, 10, 100, 1000, 10000};
        const float scaled = v * scales[decimals];
        if (!(scaled > -2.0e9f && scaled < 2.0e9f)) { raw("null"); return *this; }
        long q = static_cast<long>(scaled + (scaled >= 0 ? 0.5f : -0.5f));
        if (q < 0) { put('-'); q = -q; }
        digits(q / scales[decimals]);
        if (decimals)
        {
            put('.');
            long frac = q % scales[decimals];
            for (long s = scales[decimals] / 10; s > 0; s /= 10)
            {
                put(static_cast<char>('0' + frac / s));
                frac %= s;
            }
        }
        return *this;
    }
    JsonOut &boolean(bool v) { value(); raw(v ? "true" : "false"); return *this; }
    JsonOut &null() { value(); raw("null"); return *this; }
    JsonOut &string(const char *s) { value(); quoted(s); return *this; }

    // key and value at once
    JsonOut &field(const char *name, long long v) { key(name); return integer(v); }
    JsonOut &field(const char *name, float v, int decimals) { key(name); return number(v, decimals); }
    JsonOut &flag(const char *name, bool v) { key(name); return integer(v ? 1 : 0); }
    JsonOut &text(const char *name, const char *v) { key(name); return string(v); }

private:
    static constexpr int MAX_DEPTH = 8;
    char *buf;
    size_t cap;
    size_t len = 0;
    bool overflow = false;
    int depth = 0;
    int items[MAX_DEPTH] = {};
    bool afterKey = false;

    void put(char c)
    {
        if (len + 1 >= cap) { overflow = true; return; }
        buf[len++] = c;
        buf[len] = 0;
    }
    void raw(const char *s) { while (*s) put(*s++); }
    void quoted(const char *s)
    {
        put('"');
        for (; *s; ++s)
        {
            const unsigned char c = static_cast<unsigned char>(*s);
            if (c == '"' || c == '\\') { put('\\'); put(static_cast<char>(c)); }
            else if (c < 0x20) put(' ');
            else put(static_cast<char>(c));
        }
        put('"');
    }
    void digits(long long v)
    {
        char tmp[24];
        int n = 0;
        unsigned long long u = v < 0 ? 0ULL - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
        if (v < 0) put('-');
        do { tmp[n++] = static_cast<char>('0' + u % 10); u /= 10; } while (u && n < 23);
        while (n) put(tmp[--n]);
    }
    // A comma before every item but the first of an object or array.
    void value()
    {
        if (afterKey) { afterKey = false; return; }
        if (depth > 0 && depth <= MAX_DEPTH && items[depth - 1]++ > 0) put(',');
    }
    void open()
    {
        if (depth < MAX_DEPTH) items[depth] = 0;
        depth++;
    }
    void close()
    {
        if (depth > 0) depth--;
    }
};
