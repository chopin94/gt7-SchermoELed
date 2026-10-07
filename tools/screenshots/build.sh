#!/usr/bin/env bash
# Genera gli screenshot 320x240 di tutte le schermate del firmware dashboard.
# Requisiti: g++, gcc, git, zlib (Linux/WSL).  Uso: tools/screenshots/build.sh [cartella_output]
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
FW="$ROOT/firmware/dashboard"
OUT="${1:-$ROOT/docs/schermate}"
CACHE="$HERE/.cache"
BUILD="$CACHE/build"
mkdir -p "$CACHE" "$BUILD" "$OUT"

# Stesse versioni delle librerie usate dal firmware (platformio.ini)
[ -d "$CACHE/LovyanGFX" ] || git clone -q --depth 1 --branch 1.1.12 https://github.com/lovyan03/LovyanGFX "$CACHE/LovyanGFX"
[ -d "$CACHE/QRCode" ] || git clone -q --depth 1 https://github.com/ricmoo/QRCode "$CACHE/QRCode"

LGFX="$CACHE/LovyanGFX/src"
INCLUDES=(-I"$HERE/stubs" -I"$FW/src" -I"$FW/include" -I"$FW/lib/GT7Udp/src" -I"$FW/lib/GT7DerivedMetrics" -I"$FW/lib/OtaUpdate" -I"$LGFX" -I"$CACHE/QRCode/src")
FLAGS=(-O1 -w -DGT7_SCREENSHOT_HOST=1 -DLGFX_USE_V1 -DLGFX_LINUX_FB -DBOARD_ESP32_2432S024C=1)

# LovyanGFX: solo il codice comune e la piattaforma Linux (nessun pannello reale)
LIB_SRCS=$(find "$LGFX/lgfx/v1" -name '*.cpp' \
  -not -path '*/platforms/*' -not -path '*/panel/*' -not -path '*/touch/*' -not -path '*/LGFX_Button*')
LIB_SRCS+=" $(find "$LGFX/lgfx/v1/platforms/framebuffer" -name '*.cpp') $LGFX/lgfx/v1/panel/Panel_Device.cpp"

objs=()
for src in $LIB_SRCS "$FW/lib/GT7Udp/src/GT7UDPParser.cpp" "$FW/lib/GT7DerivedMetrics/GT7DerivedMetrics.cpp"; do
  obj="$BUILD/$(echo "$src" | md5sum | cut -c1-12).o"
  [ -f "$obj" ] || g++ -std=gnu++17 "${FLAGS[@]}" "${INCLUDES[@]}" -c "$src" -o "$obj"
  objs+=("$obj")
done
# Font della libreria (C puro): compilati una volta sola
for src in $(find "$LGFX/lgfx/Fonts" "$LGFX/lgfx/utility" -name '*.c') "$CACHE/QRCode/src/qrcode.c"; do
  obj="$BUILD/$(echo "$src" | md5sum | cut -c1-12).o"
  [ -f "$obj" ] || gcc -O0 -w -I"$LGFX" -c "$src" -o "$obj"
  objs+=("$obj")
done
g++ -std=gnu++11 "${FLAGS[@]}" "${INCLUDES[@]}" "$HERE/render.cpp" "${objs[@]}" -lz -lpthread -o "$BUILD/render"
"$BUILD/render" "$OUT"
