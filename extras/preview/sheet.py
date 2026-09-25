#!/usr/bin/env python3
"""Turns preview PPM frames into PNG contact sheets and animated GIFs.

  sheet.py sheet <dir> <out.png> [scale]     grid of sheet_XX.ppm with names
  sheet.py poses <dir> <out.png> [scale]     row of pose_XX.ppm
  sheet.py gif   <dir> <out.gif> [fps] [scale]   anim_XXXX.ppm -> GIF
"""
import glob
import os
import sys

from PIL import Image, ImageDraw, ImageFont


def load(paths, scale):
    ims = [Image.open(p).convert("RGB") for p in paths]
    if scale != 1:
        ims = [im.resize((im.width * scale, im.height * scale), Image.NEAREST) for im in ims]
    return ims


def labelled_grid(ims, labels, cols, pad=10, label_h=18, bg=(24, 24, 28)):
    w, h = ims[0].size
    rows = (len(ims) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (w + pad) + pad, rows * (h + label_h + pad) + pad), bg)
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("/System/Library/Fonts/Menlo.ttc", 13)
    except OSError:
        font = ImageFont.load_default()
    for i, im in enumerate(ims):
        x = pad + (i % cols) * (w + pad)
        y = pad + (i // cols) * (h + label_h + pad)
        sheet.paste(im, (x, y + label_h))
        if labels:
            draw.text((x, y + 2), labels[i], fill=(210, 210, 215), font=font)
    return sheet


def main():
    mode, d, out = sys.argv[1], sys.argv[2], sys.argv[3]
    if mode == "sheet":
        scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
        paths = sorted(glob.glob(os.path.join(d, "sheet_*.ppm")))
        names = {}
        names_file = os.path.join(d, "names.txt")
        if os.path.exists(names_file):
            for line in open(names_file):
                k, _, v = line.strip().partition(" ")
                names[int(k)] = v
        labels = [f"{i:02d} {names.get(i, '')}" for i in range(len(paths))]
        labelled_grid(load(paths, scale), labels, cols=4).save(out)
    elif mode == "poses":
        scale = int(sys.argv[4]) if len(sys.argv) > 4 else 1
        paths = sorted(glob.glob(os.path.join(d, "pose_*.ppm")))
        labels = ["straight", "up-right", "down-left", "half blink",
                  "angry", "suspicious", "sleepy", "startled"][: len(paths)]
        labelled_grid(load(paths, scale), labels, cols=2).save(out)
    elif mode == "gif":
        fps = int(sys.argv[4]) if len(sys.argv) > 4 else 30
        scale = int(sys.argv[5]) if len(sys.argv) > 5 else 1
        paths = sorted(glob.glob(os.path.join(d, "anim_*.ppm")))
        ims = load(paths, scale)
        ims[0].save(out, save_all=True, append_images=ims[1:], duration=int(1000 / fps), loop=0)
    print(out)


if __name__ == "__main__":
    main()
