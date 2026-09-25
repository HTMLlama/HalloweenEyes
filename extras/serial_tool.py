#!/usr/bin/env python3
"""Talk to HaloweenEyesV2 over USB serial.

  serial_tool.py log [seconds] [cmd ...]   send commands, print output
  serial_tool.py dump out.png [scale]      grab the left eye's framebuffer

Commands are the single-letter console commands (send '?' for the list);
use e.g. "7\\n" to jump to style 7. Needs pyserial (and Pillow for dump).
"""
import glob
import sys
import time

import serial


def open_port():
    ports = sorted(glob.glob("/dev/cu.usbmodem*")) or sorted(glob.glob("/dev/ttyACM*"))
    if not ports:
        sys.exit("No Feather found on USB")
    s = serial.Serial(ports[0], 115200, timeout=0.2)
    time.sleep(0.3)
    s.reset_input_buffer()
    return s


def log(seconds, cmds):
    s = open_port()
    for c in cmds:
        s.write(c.encode().decode("unicode_escape").encode())
        time.sleep(0.25)
    end = time.time() + seconds
    while time.time() < end:
        data = s.read(4096)
        if data:
            sys.stdout.write(data.decode(errors="replace"))
            sys.stdout.flush()


def dump(path, scale):
    from PIL import Image

    s = open_port()
    s.write(b"d")
    buf = b""
    need = 128 * 128 * 2
    end = time.time() + 5
    raw = None
    while time.time() < end:
        buf += s.read(65536)
        start = buf.find(b"FRAME 128 128\r\n")
        if start >= 0 and len(buf) >= start + 15 + need:
            raw = buf[start + 15 : start + 15 + need]  # read by length: pixels may contain any bytes
            break
    if raw is None:
        sys.exit("No frame received")
    im = Image.new("RGB", (128, 128))
    px = im.load()
    for i in range(128 * 128):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]  # big-endian RGB565
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        px[i % 128, i // 128] = ((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2))
    if scale > 1:
        im = im.resize((128 * scale, 128 * scale), Image.NEAREST)
    im.save(path)
    print(path)


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "dump":
        dump(sys.argv[2] if len(sys.argv) > 2 else "frame.png",
             int(sys.argv[3]) if len(sys.argv) > 3 else 3)
    else:
        secs = float(sys.argv[2]) if len(sys.argv) > 2 else 6
        log(secs, sys.argv[3:])
