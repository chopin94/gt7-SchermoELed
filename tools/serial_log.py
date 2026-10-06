"""Reset the board via RTS (EN) and capture its serial output.

Usage: python serial_log.py [seconds] [outfile] [--no-reset] [--baud=N]
"""
import sys
import time

import serial

duration = float(sys.argv[1]) if len(sys.argv) > 1 else 12
outfile = sys.argv[2] if len(sys.argv) > 2 and not sys.argv[2].startswith("--") else None
reset = "--no-reset" not in sys.argv
baud = next((int(a.split("=", 1)[1]) for a in sys.argv if a.startswith("--baud=")), 115200)

s = serial.Serial()
s.port = "COM4"
s.baudrate = baud
s.timeout = 0.2
s.dtr = False  # IO0 high -> normal boot
s.rts = False  # EN released
s.open()
if reset:
    s.rts = True
    time.sleep(0.15)
    s.rts = False

out = open(outfile, "wb") if outfile else None
data = b""
t0 = time.time()
while time.time() - t0 < duration:
    chunk = s.read(4096)
    if chunk:
        data += chunk
        if out:
            out.write(chunk)
            out.flush()
s.close()
if out:
    out.close()
sys.stdout.write(data.decode("utf-8", "replace"))
