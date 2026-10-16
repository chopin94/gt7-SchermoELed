#!/usr/bin/env python3
"""Genera firmware/led-strip/src/DashAssets.h: icone per la schermata Home,
font dei numeri e video muto per tenere acceso lo schermo del telefono.

    python3 tools/images/cruscotto_assets.py

I file di partenza stanno in tools/images/cruscotto:
- BarlowCondensed-*-sub.woff2: Barlow Condensed (licenza OFL, OFL-BarlowCondensed.txt)
  ridotta a cifre e lettere con
  pyftsubset BarlowCondensed-Bold.ttf --text="0123456789:.,+-−±°%/ A-Za-z▲×" --flavor=woff2 --layout-features=tnum,kern --no-hinting
- sveglia.mp4 / sveglia.webm: 2 s di nero 16x16, con
  ffmpeg -f lavfi -i color=c=black:s=16x16:r=1 -t 2 -c:v libx264 -profile:v baseline -pix_fmt yuv420p -movflags +faststart sveglia.mp4
  ffmpeg -f lavfi -i color=c=black:s=16x16:r=1 -t 2 -c:v libvpx -b:v 10k sveglia.webm
Le icone sono disegnate qui con Pillow, con la scritta in BarlowCondensed-Bold.ttf.
"""
import base64
import io
import math
import pathlib

from PIL import Image, ImageDraw, ImageFont

ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC = ROOT / "tools/images/cruscotto"
OUT = ROOT / "firmware/led-strip/src/DashAssets.h"


def icon(size):
    s = 4 * size  # disegnata grande e poi ridotta, per bordi morbidi
    im = Image.new("RGB", (s, s), (0, 0, 0))
    d = ImageDraw.Draw(im)
    cx, cy, r = s / 2, s * 0.56, s * 0.34
    w = int(s * 0.07)
    box = [cx - r, cy - r, cx + r, cy + r]
    d.arc(box, 135, 315, fill=(241, 242, 245), width=w)
    d.arc(box, 315, 405, fill=(255, 43, 28), width=w)
    a = math.radians(-60)
    d.line([cx, cy, cx + math.cos(a) * r * 0.95, cy + math.sin(a) * r * 0.95], fill=(255, 43, 28), width=int(s * 0.035))
    font = ImageFont.truetype(str(SRC / "BarlowCondensed-Bold.ttf"), int(s * 0.30))
    d.text((cx, cy + r * 0.55), "GT7", font=font, fill=(241, 242, 245), anchor="mm")
    im = im.resize((size, size), Image.LANCZOS).quantize(colors=32)
    buf = io.BytesIO()
    im.save(buf, "PNG", optimize=True)
    return buf.getvalue()


def c_array(name, data):
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 20]) for i in range(0, len(data), 20)]
    return f"static const uint8_t {name}[] PROGMEM = {{\n    " + ",\n    ".join(rows) + "\n};\n"


def main():
    parts = [
        "#pragma once\n#include <pgmspace.h>\n#include <stdint.h>\n\n"
        "// Generato da tools/images/cruscotto_assets.py: non modificare a mano.\n"
        "// Icone della schermata Home e font dei numeri del cruscotto (/cruscotto).\n"
        "// Barlow Condensed: SIL Open Font License 1.1 (tools/images/cruscotto).\n\n"
    ]
    for size in (180, 192, 512):
        parts.append(c_array(f"DASH_ICON_{size}", icon(size)))
    parts.append(c_array("DASH_FONT_BOLD", (SRC / "BarlowCondensed-Bold-sub.woff2").read_bytes()))
    parts.append(c_array("DASH_FONT_SEMIBOLD", (SRC / "BarlowCondensed-SemiBold-sub.woff2").read_bytes()))
    # Il cruscotto scarica il video con fetch() e lo riproduce da un blob:
    # Safari vuole le richieste a intervalli (Range) per i video, che il
    # WebServer non gestisce.
    parts.append(c_array("DASH_WAKE_MP4", (SRC / "sveglia.mp4").read_bytes()))
    parts.append(c_array("DASH_WAKE_WEBM", (SRC / "sveglia.webm").read_bytes()))
    OUT.write_text("\n".join(parts))
    print(OUT, OUT.stat().st_size, "byte")


if __name__ == "__main__":
    main()
