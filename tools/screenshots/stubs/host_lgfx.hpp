// Host replacement for src/board/LGFX_ESP32_esp32-2432s028.hpp: the "panel" is
// a 320x240 RGB565 sprite in memory that the renderer saves as PNG.
#pragma once
#include <LovyanGFX.hpp>

static constexpr uint8_t DASHBOARD_DISPLAY_ROTATION = 1;
static constexpr uint8_t DASHBOARD_TOUCH_ROTATION_OFFSET = 0;

class LGFX : public lgfx::LGFX_Sprite {
public:
    bool init()
    {
        setColorDepth(16);
        return createSprite(320, 240) != nullptr;
    }
    // The dashboard is always landscape 320x240 here.
    void setRotation(uint_fast8_t) {}
    void setBrightness(uint8_t value) { brightness_ = value; }
    uint8_t getBrightness() const { return brightness_; }
    template <typename T> bool getTouch(T *, T *) { return false; }
    void wakeup() {}
    void sleep() {}

private:
    uint8_t brightness_ = 255;
};
