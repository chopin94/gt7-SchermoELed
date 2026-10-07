#pragma once

inline constexpr char GT7_DASH_VERSION[] = "2.1.0";

// Board the firmware is built for (PlatformIO environment): the update over
// Wi-Fi accepts only a firmware.bin built for the same one.
#if defined(BOARD_ESP32_2432S024C)
#define GT7_DASH_BOARD "esp32-2432s024c"
#elif defined(DISPLAY_PANEL_ST7789)
#define GT7_DASH_BOARD "esp32-st7789"
#else
#define GT7_DASH_BOARD "esp32"
#endif
