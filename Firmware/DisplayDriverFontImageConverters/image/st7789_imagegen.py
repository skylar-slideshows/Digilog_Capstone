#!/usr/bin/env python3
"""
st7789_imagegen.py: turns a picture into a header the screens can draw with
displays_draw_image(), one image pixel to one screen pixel. Nothing is scaled.

    pip install "pillow>=10.1"
    python st7789_imagegen.py logo.png --preview logo_preview.png

Colours are reduced to what the screens can show: 16-bit (RGB565, 65,536 colours) or 12-bit
(RGB444, 4,096 colours). Use the same --bits as DISPLAY_COLOR_BITS in displays.h.

--dither spreads each pixel's rounding error over its neighbours (Floyd-Steinberg). That hides
the banding in photos and smooth gradients, especially at 12-bit; leave it off for flat-colour
graphics such as icons and text, where it adds speckle.

Pixels are stored exactly as the screen receives them, row by row, so drawing one is a straight
copy with no conversion. Transparent pixels are blended onto --background (black by default).
The --preview image is decoded from those stored bytes, so it shows exactly what the screen will.
"""

import argparse
import os
import re
import sys

from PIL import Image, ImageOps

SCREEN = 240
LEVELS = {16: (31, 63, 31), 12: (15, 15, 15)}  # top value of each channel: red, green, blue


def load(path, background):
    img = ImageOps.exif_transpose(Image.open(path))  # photos can be stored sideways
    if img.mode in ("RGBA", "LA", "PA") or "transparency" in img.info:
        img = img.convert("RGBA")
        canvas = Image.new("RGBA", img.size, background + (255,))
        img = Image.alpha_composite(canvas, img)
    return img.convert("RGB")


def quantize(img, bits, dither):
    """Each pixel's channels reduced to the screen's levels, row by row."""
    levels = LEVELS[bits]
    w, h = img.size
    raw = img.tobytes()  # R, G, B, R, G, B...
    px = [[float(raw[i]), float(raw[i + 1]), float(raw[i + 2])] for i in range(0, len(raw), 3)]
    out = []
    for i, p in enumerate(px):
        q = []
        for c in range(3):
            top = levels[c]
            v = min(255.0, max(0.0, p[c]))
            level = round(v * top / 255)
            q.append(level)
            if dither:
                err = v - level * 255 / top
                x, y = i % w, i // w
                for dx, dy, share in ((1, 0, 7), (-1, 1, 3), (0, 1, 5), (1, 1, 1)):
                    nx, ny = x + dx, y + dy
                    if 0 <= nx < w and ny < h:
                        px[ny * w + nx][c] += err * share / 16
        out.append(q)
    return out


def pack(pixels, bits):
    """The screen's own byte format. 16-bit: two bytes per pixel, high byte first.
    12-bit: three bytes per two pixels, RG BR GB; an odd last pixel takes two bytes."""
    data = bytearray()
    if bits == 16:
        for r, g, b in pixels:
            v = (r << 11) | (g << 5) | b
            data += bytes((v >> 8, v & 0xFF))
    else:
        for i in range(0, len(pixels) - 1, 2):
            (r1, g1, b1), (r2, g2, b2) = pixels[i], pixels[i + 1]
            data += bytes(((r1 << 4) | g1, (b1 << 4) | r2, (g2 << 4) | b2))
        if len(pixels) % 2:
            r, g, b = pixels[-1]
            data += bytes(((r << 4) | g, b << 4))
    return bytes(data)


def unpack(data, bits, count):
    """The stored bytes back to 8-bit RGB, the way the screen widens them."""
    out = []
    if bits == 16:
        for i in range(count):
            v = (data[2 * i] << 8) | data[2 * i + 1]
            r, g, b = v >> 11, (v >> 5) & 0x3F, v & 0x1F
            out.append(((r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)))
    else:
        nibbles = []
        for byte in data:
            nibbles += [byte >> 4, byte & 0xF]
        for i in range(count):
            r, g, b = nibbles[3 * i: 3 * i + 3]
            out.append((r * 17, g * 17, b * 17))
    return out


def c_name(text):
    name = re.sub(r"\W", "_", text)
    return name if re.match(r"[A-Za-z_]", name) else "img_" + name


def write_header(path, name, source, w, h, bits, data):
    lines = [
        "/**",
        f" * {os.path.basename(path)}: {os.path.basename(source)} for displays_draw_image(), made by st7789_imagegen.py.",
        f" * {w} x {h} pixels, {bits}-bit colour, {len(data):,} bytes of flash.",
        " *",
        " * Include this in only one .c file: each file that includes it gets its own copy in flash.",
        " */",
        "",
        f"#ifndef {name.upper()}_IMAGE_H",
        f"#define {name.upper()}_IMAGE_H",
        "",
        '#include "FreeRTOS_apps/displays.h"',
        "",
        f"static const uint8_t {name}_pixels[{len(data)}] = {{",
    ]
    for i in range(0, len(data), 16):
        lines.append("    " + ", ".join(f"0x{v:02X}" for v in data[i:i + 16]) + ",")
    lines += [
        "};",
        "",
        f"static const display_image_t {name} = {{{w}, {h}, {bits}, {name}_pixels}};",
        "",
        "#endif",
        "",
    ]
    with open(path, "w", encoding="ascii", errors="replace", newline="\n") as f:
        f.write("\n".join(lines))


def main():
    p = argparse.ArgumentParser(description="Convert a picture for displays_draw_image().")
    p.add_argument("image", help="picture file: PNG, JPEG, BMP, GIF and most others")
    p.add_argument("-o", "--output", help="header to write (default: the picture's name with .h)")
    p.add_argument("--name", help="C name to draw it by (default: from the file name)")
    p.add_argument("--bits", type=int, choices=(16, 12), default=16,
                   help="colour depth: must match DISPLAY_COLOR_BITS in displays.h (default: %(default)s)")
    p.add_argument("--dither", action="store_true", help="hide banding in photos and gradients")
    p.add_argument("--background", default="#000000", metavar="#RRGGBB",
                   help="colour behind transparent pixels (default: %(default)s)")
    p.add_argument("--preview", metavar="PNG", help="also save the picture exactly as the screen will show it")
    args = p.parse_args()

    if not re.fullmatch(r"#[0-9A-Fa-f]{6}", args.background):
        sys.exit("--background must look like #RRGGBB")
    background = tuple(int(args.background[i:i + 2], 16) for i in (1, 3, 5))

    try:
        img = load(args.image, background)
    except OSError as e:
        sys.exit(f"can't read {args.image}: {e}")
    w, h = img.size
    if w > SCREEN or h > SCREEN:
        sys.exit(f"{args.image} is {w} x {h}, but the screens are {SCREEN} x {SCREEN} and pictures "
                 "aren't scaled: resize it first")

    stem = os.path.splitext(os.path.basename(args.image))[0]
    output = args.output or stem + ".h"
    name = c_name(args.name or stem)

    pixels = quantize(img, args.bits, args.dither)
    data = pack(pixels, args.bits)
    write_header(output, name, args.image, w, h, args.bits, data)
    print(f"{os.path.basename(args.image)}: {w} x {h}, {args.bits}-bit colour, "
          f"{len(data):,} bytes of flash ({len(data) / (512 * 1024):.1%} of 512 KB)")
    print(f"wrote {output}; draw it with displays_draw_image(display, x, y, &{name})")

    if args.preview:
        rgb = bytes(v for pixel in unpack(data, args.bits, w * h) for v in pixel)
        Image.frombytes("RGB", (w, h), rgb).save(args.preview)
        print(f"wrote {args.preview}")


if __name__ == "__main__":
    main()
