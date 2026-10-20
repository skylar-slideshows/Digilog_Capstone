#!/usr/bin/env python3
"""
st7789_fontgen.py: turns any TrueType or OpenType font into driver_st7789_font.h for the
LibDriver ST7789 driver.

The driver draws the ASCII characters ' ' to '~' in three fixed sizes, 12, 16 and 24 pixels
tall, and each character cell is half as wide as it is tall. This renders a font into exactly
that format, so the file it writes replaces driver_st7789_font.h with no other changes.

    pip install "pillow>=10.1"
    python st7789_fontgen.py MyFont.ttf --preview preview.png

How a font is fitted to the cells (--fit):
  shrink    (default) The font is scaled evenly until its digits fit the cell width, then
            drawn by FreeType's monochrome renderer, which snaps every stroke to whole pixels.
            Crisp 1-pixel strokes; letters use a little less of the cell height.
  condense  The font fills the cell height and is narrowed evenly until its digits fit. That
            narrowing needs greyscale drawing, so --threshold sets how bold the result is.
            Taller letters, but at 12 px thin strokes can break up.

Either way, all characters share one baseline and keep the font's own spacing where they fit.
Any still too wide for the cell, which happens with proportional fonts, are squeezed to leave
a one-pixel gap. Use --preview to compare: every font behaves a little differently.

Font size (--size12, --size16, --size24):
  By default each table uses the largest font size that fits its cells, and prints the size
  it chose. Set one yourself to override it, for example --size16 15. Larger sizes make bigger
  letters, but characters that no longer fit are squeezed or cut off at the top or bottom;
  both are listed when it runs, and --preview shows the result.
"""

import argparse
import os
import sys

from PIL import Image, ImageDraw, ImageFont

SIZES = (12, 16, 24)
NAMES = {12: "gsc_st7789_ascii_1206", 16: "gsc_st7789_ascii_1608", 24: "gsc_st7789_ascii_2412"}
CHARS = [chr(c) for c in range(32, 127)]  # the 95 characters the driver can draw


# ------------------------------------------------------------------ the driver's bit format

def pack(cell, size):
    """Pixels -> the driver's bytes: column by column from the left, each column top to bottom,
    most significant bit first, padded to whole bytes."""
    width, per_column = size // 2, (size + 7) // 8
    out = []
    for x in range(width):
        for b in range(per_column):
            byte = 0
            for i in range(8):
                y = b * 8 + i
                if y < size and cell[y][x]:
                    byte |= 0x80 >> i
            out.append(byte)
    return out


def unpack(data, size):
    """The driver's bytes -> pixels, using the same loop as a_st7789_show_char."""
    width = size // 2
    cell = [[False] * width for _ in range(size)]
    x = y = 0
    for t in range((size // 8 + (1 if size % 8 else 0)) * width):
        temp = data[t]
        for _ in range(8):
            if temp & 0x80:
                cell[y][x] = True
            temp = (temp << 1) & 0xFF
            y += 1
            if y == size:
                y, x = 0, x + 1
                break
    return cell


# ------------------------------------------------------------------ rendering

def render(font, ch, size, mono):
    """Draws one character on a roomy canvas. Returns the canvas, the x of the character's
    origin and the y of its baseline, which are the same for every character."""
    canvas = Image.new("L", (size * 6, size * 5), 0)
    ox, base = size * 2, size * 3
    draw = ImageDraw.Draw(canvas)
    if mono:
        draw.fontmode = "1"  # FreeType's monochrome renderer: strokes snapped to whole pixels
    draw.text((ox, base), ch, font=font, fill=255, anchor="ls")
    return canvas, ox, base


def ink(img, threshold):
    """Bounding box of the pixels dark enough to count as on, or None for an empty character."""
    return img.point(lambda v: 255 if v >= threshold else 0).getbbox()


def vertical_extent(font, size, threshold, mono):
    """How far the font's ink reaches above and below the baseline, across all 95 characters."""
    top = bottom = 0
    for ch in CHARS:
        img, _, base = render(font, ch, size, mono)
        box = ink(img, threshold)
        if box:
            top, bottom = min(top, box[1] - base), max(bottom, box[3] - base)
    return top, bottom


def fit(path, size, threshold, mode, chosen=None):
    """Font scale for the cells, and how much to narrow it (1.0 = not at all)."""
    mono = mode == "shrink"
    if chosen:  # the size was set on the command line: use it as given
        font = ImageFont.truetype(path, chosen)
        narrow = min(1.0, (size // 2) / font.getlength("0")) if mode == "condense" else 1.0
        return font, chosen, narrow, vertical_extent(font, size, threshold, mono)
    lo, hi = size * 0.3, size * 3.0
    for _ in range(18):  # largest scale at which every character fits the cell height
        mid = (lo + hi) / 2
        top, bottom = vertical_extent(ImageFont.truetype(path, mid), size, threshold, mono)
        lo, hi = (mid, hi) if bottom - top <= size else (lo, mid)
    em, width = lo, size // 2
    font = ImageFont.truetype(path, em)
    narrow = min(1.0, width / font.getlength("0"))  # digits: the same width in nearly every font
    if mode == "shrink" and narrow < 1.0:
        em, narrow = em * narrow, 1.0
        font = ImageFont.truetype(path, em)
        while font.getlength("0") > width and em > 1:  # hinting rounds widths; make sure
            em -= 0.05
            font = ImageFont.truetype(path, em)
    return font, em, narrow, vertical_extent(font, size, threshold, mono)


def make_table(path, size, threshold, mode, chosen=None):
    """All 95 characters at one size: their bytes, plus what fitting them took."""
    font, em, narrow, (top, bottom) = fit(path, size, threshold, mode, chosen)
    width = size // 2
    band_top = None  # canvas row that becomes the cell's top row: the same for every character
    table, squeezed, clipped = [], [], []

    for ch in CHARS:
        img, ox, base = render(font, ch, size, mode == "shrink")
        if band_top is None:
            band_top = base + top - (size - (bottom - top)) // 2  # centre the font's ink vertically
        cell = Image.new("L", (width, size), 0)
        box = ink(img, threshold)
        if box:
            if box[1] < band_top or box[3] > band_top + size:  # only when a size was set too big
                clipped.append(ch)
            left, right = box[0], box[2]
            glyph = img.crop((left, band_top, right, band_top + size))
            w = right - left
            if narrow < 1.0:
                w = max(1, round(w * narrow))
                glyph = glyph.resize((w, size), Image.Resampling.LANCZOS)
            # where the font's own spacing would put it, centred on its advance width
            x = round((width - font.getlength(ch) * narrow) / 2 + (left - ox) * narrow)
            if x < 0 or x + w > width:
                if w > width - 1:  # squeeze, leaving a one-pixel gap to the next character
                    glyph = glyph.resize((width - 1, size), Image.Resampling.LANCZOS)
                    w = width - 1
                    squeezed.append(ch)
                x = (width - w) // 2
            cell.paste(glyph, (x, 0))
        bits = [[cell.getpixel((cx, cy)) >= threshold for cx in range(width)] for cy in range(size)]
        table.append(pack(bits, size))

    return table, em, narrow, squeezed, clipped


# ------------------------------------------------------------------ output

def describe(font_path, em, chosen, narrow, squeezed, clipped):
    """One line on how a table was made, for the console and the header comment."""
    text = f"{os.path.basename(font_path)} at {em:.1f} px" + (" (set)" if chosen else "")
    if narrow < 1.0:
        text += f", narrowed to {narrow:.0%}"
    if squeezed:
        text += f", squeezed {len(squeezed)} to fit: {''.join(squeezed)}"
    if clipped:
        text += f", cut off at the top or bottom: {''.join(clipped)}"
    return text


def write_header(path, tables, sources, mode):
    lines = [
        "/**",
        " * driver_st7789_font.h, generated by st7789_fontgen.py. Replaces the original file",
        " * as is: same three tables, same names, same layout.",
        " *",
        f" * --fit {mode}",
    ]
    for size in SIZES:
        lines.append(f" * {size:>2} px: {describe(*sources[size])}")
    lines += [
        " */",
        "",
        "#ifndef DRIVER_ST7789_FONT_H",
        "#define DRIVER_ST7789_FONT_H",
        "",
        "#include <stdint.h>",
        "#include <stdio.h>",
        "#include <string.h>",
        "",
        "#ifdef __cplusplus",
        'extern "C"{',
        "#endif",
        "",
    ]
    for size in SIZES:
        per_char = len(tables[size][0])
        lines.append(f"static const uint8_t {NAMES[size]}[95][{per_char}] =")
        lines.append("{")
        for i, (ch, data) in enumerate(zip(CHARS, tables[size])):
            hexes = ", ".join(f"0x{v:02X}" for v in data)
            lines.append(f'    {{{hexes}}}, /*"{ch}", {i}*/')
        lines.append("};")
        lines.append("")
    lines += ["#ifdef __cplusplus", "}", "#endif", "", "#endif", ""]
    with open(path, "w", encoding="ascii", errors="replace", newline="\n") as f:  # font names may not be ASCII
        f.write("\n".join(lines))


def write_preview(path, tables, scale=3):
    """Every character at every size, decoded from the generated bytes exactly as the driver
    draws them, placed side by side the way it spaces them."""
    rows = []
    for size in SIZES:
        width = size // 2
        for chunk in (CHARS[:48], CHARS[48:]):
            row = Image.new("L", (48 * width, size), 0)
            for i, ch in enumerate(chunk):
                cell = unpack(tables[size][ord(ch) - 32], size)
                for y in range(size):
                    for x in range(width):
                        if cell[y][x]:
                            row.putpixel((i * width + x, y), 255)
            rows.append(row)
    gap = 6
    sheet = Image.new("L", (max(r.width for r in rows) + 2 * gap, sum(r.height + gap for r in rows) + gap), 40)
    y = gap
    for r in rows:
        sheet.paste(r, (gap, y))
        y += r.height + gap
    sheet.resize((sheet.width * scale, sheet.height * scale), Image.Resampling.NEAREST).save(path)


# ------------------------------------------------------------------ command line

def main():
    p = argparse.ArgumentParser(description="Convert a TrueType/OpenType font for the LibDriver ST7789 driver.")
    p.add_argument("font", help="font file (.ttf or .otf) used for all three sizes")
    p.add_argument("-o", "--output", default="driver_st7789_font.h", help="header to write (default: %(default)s)")
    p.add_argument("--preview", metavar="PNG", help="also save an image of every character as the driver will draw it")
    p.add_argument("--fit", choices=("shrink", "condense"), default="shrink",
                   help="shrink: crisp, smaller letters. condense: taller, narrowed letters (default: %(default)s)")
    p.add_argument("--threshold", type=int, default=128, metavar="1-255",
                   help="for --fit condense and squeezed characters: how dark a pixel must be to "
                        "light up; lower makes text bolder (default: %(default)s)")
    for size in SIZES:
        p.add_argument(f"--size{size}", type=float, metavar="PX",
                       help=f"font size for the {size} px characters (default: the largest that fits)")
    for size in SIZES:
        p.add_argument(f"--font{size}", metavar="FILE", help=f"different font for the {size} px size")
    args = p.parse_args()

    if not 1 <= args.threshold <= 255:
        sys.exit("--threshold must be between 1 and 255")
    for size in SIZES:
        chosen = getattr(args, f"size{size}")
        if chosen is not None and not 1 <= chosen <= size * 3:
            sys.exit(f"--size{size} must be between 1 and {size * 3}")

    tables, sources = {}, {}
    for size in SIZES:
        path = getattr(args, f"font{size}") or args.font
        chosen = getattr(args, f"size{size}")
        try:
            table, em, narrow, squeezed, clipped = make_table(path, size, args.threshold, args.fit, chosen)
        except OSError as e:
            sys.exit(f"can't read {path}: {e}")
        tables[size], sources[size] = table, (path, em, chosen, narrow, squeezed, clipped)
        print(f"{size:>2} px: {describe(*sources[size])}")

    write_header(args.output, tables, sources, args.fit)
    print(f"wrote {args.output}")
    if args.preview:
        write_preview(args.preview, tables)
        print(f"wrote {args.preview}")


if __name__ == "__main__":
    main()
