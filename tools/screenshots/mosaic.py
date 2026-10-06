"""Compone docs/screenshots/mosaico-temi-v2.png (immagine di apertura del README).
Uso: python3 tools/screenshots/mosaic.py  (richiede Pillow, dopo build.sh)"""
from pathlib import Path
from PIL import Image

SHOTS = Path(__file__).resolve().parents[2] / "docs" / "screenshots"
THEMES = ["classic", "gt3", "retro", "radar", "mono", "pocket", "endurance",
          "ferrari", "ferrari-ac", "ferrari-gold", "bmw-m"]
W, H, GAP, COLS, ROWS = 320, 240, 12, 4, 3

sheet = Image.new("RGB", (COLS * W + (COLS + 1) * GAP, ROWS * H + (ROWS + 1) * GAP), (13, 15, 20))
tiles = [("theme-" + name + ("-v2" if name in ("ferrari-gold", "bmw-m") else "") + ".png") for name in THEMES] + ["waiting-racing.png"]
for i, tile in enumerate(tiles):
    sheet.paste(Image.open(SHOTS / tile), (GAP + (i % COLS) * (W + GAP), GAP + (i // COLS) * (H + GAP)))
sheet.save(SHOTS / "mosaico-temi-v2.png", optimize=True)
print(SHOTS / "mosaico-temi-v2.png")
