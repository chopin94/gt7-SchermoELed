"""Converte un'immagine in un header C RGB565 da mostrare sullo schermo con tft.pushImage().

Stesso formato di firmware/dashboard/src/sparco_racing_bg.h e sparco_logo.h
(RGB565 con byte scambiati, come li vuole LovyanGFX per gli array uint16_t).

Esempi:
  python3 tools/images/image_to_header.py sfondo.png sparco_racing_bg --size 320x240 --crop
  python3 tools/images/image_to_header.py logo.png sparco_logo --size 300x78
Il file <nome>.h viene scritto nella cartella corrente (copiarlo in firmware/dashboard/src/).
"""
import argparse
from PIL import Image


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("image")
    parser.add_argument("name", help="nome della variabile C, es. sparco_racing_bg")
    parser.add_argument("--size", default="320x240", help="larghezza x altezza finale (default 320x240)")
    parser.add_argument("--crop", action="store_true", help="ritaglia al centro per mantenere le proporzioni")
    args = parser.parse_args()

    width, height = (int(v) for v in args.size.lower().split("x"))
    img = Image.open(args.image).convert("RGB")
    if args.crop:
        w, h = img.size
        target = width / height
        if w / h > target:
            new_w = int(h * target)
            img = img.crop(((w - new_w) // 2, 0, (w - new_w) // 2 + new_w, h))
        else:
            new_h = int(w / target)
            img = img.crop((0, (h - new_h) // 2, w, (h - new_h) // 2 + new_h))
    img = img.resize((width, height), Image.Resampling.LANCZOS)

    values = []
    for y in range(height):
        for x in range(width):
            r, g, b = img.getpixel((x, y))
            c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            values.append(f"0x{((c & 0xFF) << 8) | (c >> 8):04X}")

    with open(f"{args.name}.h", "w") as f:
        f.write(f"// {args.image} ({width}x{height}), generato da tools/images/image_to_header.py\n")
        f.write(f"const uint16_t {args.name}_width = {width};\n")
        f.write(f"const uint16_t {args.name}_height = {height};\n")
        f.write(f"const uint16_t {args.name}[] PROGMEM = {{\n")
        for i in range(0, len(values), 16):
            f.write("  " + ", ".join(values[i:i + 16]) + ",\n")
        f.write("};\n")
    print(f"{args.name}.h ({width}x{height})")


if __name__ == "__main__":
    main()
