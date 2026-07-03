#!/usr/bin/env python3
"""Prepare an image for the 4-level grayscale e-paper.

Scales/crops to 800x480, optimizes contrast, and Floyd-Steinberg-dithers to
exactly the 4 panel gray levels (0, 85, 170, 255).

Usage: python3 tools/dither4.py input.jpg output.png [--rotate]
"""

import sys

from PIL import Image, ImageOps

WIDTH, HEIGHT = 800, 480
LEVELS = [0, 85, 170, 255]


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        sys.exit(__doc__)
    src, dst = args

    im = Image.open(src)
    im = ImageOps.exif_transpose(im)
    if "--rotate" in sys.argv:
        im = im.rotate(90, expand=True)

    # Fit to target size (crop, don't distort) and stretch contrast —
    # everything looks flatter on e-paper than on a monitor.
    im = ImageOps.fit(im.convert("L"), (WIDTH, HEIGHT))
    im = ImageOps.autocontrast(im, cutoff=1)

    # Floyd-Steinberg dither onto a 4-gray palette
    pal = Image.new("P", (1, 1))
    pal.putpalette(sum(([v, v, v] for v in LEVELS), []) + [0] * (256 - 4) * 3)
    quant = im.convert("RGB").quantize(palette=pal, dither=Image.Dither.FLOYDSTEINBERG)

    # Palette indices -> actual gray values, saved as 8-bit PNG
    out = quant.point(lambda i: LEVELS[i] if i < 4 else 255, mode="L")
    out.save(dst)
    print(f"OK: {dst} ({WIDTH}x{HEIGHT}, 4 gray levels)")


if __name__ == "__main__":
    main()
