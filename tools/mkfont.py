"""Pre-render a TrueType font into an LVGL v9 bitmap font (C source).

Watch firmware ships fonts as pre-rendered bitmaps at each size it uses, so
drawing text costs no CPU. This does the same: 4 bits per pixel (16 levels of
anti-aliasing), the format of lv_font_fmt_txt. No npm tooling needed.

  python3 tools/mkfont.py FONT.ttf SIZE NAME [--range 32-126] [--tabular] > sdk/fonts/NAME.c

SIZE is the em size in pixels. --range picks the contiguous code-point range
(default printable ASCII); big number fonts use 32-58 (space..colon) to save
flash. Needs Pillow with FreeType.
"""
from __future__ import annotations
import argparse
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

LEVELS = np.array([0, 85, 170, 255])
HINT_MAX_SIZE = 40          # grid-fit glyphs up to this em size (px)
SS = 4                      # supersampling factor for grid fitting


def render(font: ImageFont.FreeTypeFont, ch: str):
    """Return (adv_w_16ths, box_w, box_h, ofs_x, ofs_y, 4bpp values row-major)."""
    adv = font.getlength(ch)
    x0, y0, x1, y1 = font.getbbox(ch, anchor="ls")          # origin: left, baseline
    if x1 <= x0 or y1 <= y0:
        return round(adv * 16), 0, 0, 0, 0, []
    img = Image.new("L", (x1 - x0, y1 - y0), 0)
    ImageDraw.Draw(img).text((-x0, -y0), ch, font=font, fill=255, anchor="ls")
    w, h = img.size
    px = list(quantize(np.frombuffer(img.tobytes(), np.uint8)))
    rows = [px[r * w:(r + 1) * w] for r in range(h)]
    # trim empty borders so boxes are tight (keeps flash small)
    top = next((i for i, r in enumerate(rows) if any(r)), None)
    if top is None:
        return round(adv * 16), 0, 0, 0, 0, []
    bot = max(i for i, r in enumerate(rows) if any(r))
    cols = [any(rows[r][c] for r in range(h)) for c in range(w)]
    left = cols.index(True)
    right = w - 1 - cols[::-1].index(True)
    rows = [r[left:right + 1] for r in rows[top:bot + 1]]
    box_w, box_h = right - left + 1, bot - top + 1
    ofs_x = x0 + left
    bottom_y = y0 + bot + 1                                   # first row below the ink, y down from baseline
    ofs_y = -bottom_y                                         # LVGL: box bottom above baseline, y up
    vals = [min(15, (v * 15 + 127) // 255) for r in rows for v in r]
    return round(adv * 16), box_w, box_h, ofs_x, ofs_y, vals


def quantize(v):
    """Display-aware anti-aliasing: the 64-colour screen shows 4 levels per
    channel, so snap coverage to those and drop faint haze (< 25%), which
    would otherwise become stray dark specks."""
    v = np.asarray(v, np.int32)
    return np.where(v < 64, 0, np.where(v < 128, 85, np.where(v < 213, 170, 255)))


def render_fitted(big: ImageFont.FreeTypeFont, ch: str):
    """Grid-fitted small glyph ("hinting"): render at SS x, try every
    sub-pixel shift within +-1/2 px, keep the crispest, then box-downsample.
    Removes the blur and stray tails (e.g. a round colon dot that overshoots
    the baseline smearing into a comma) that plain rendering gives at small
    sizes. Returns the same tuple as render()."""
    adv = big.getlength(ch) / SS
    x0, y0, x1, y1 = big.getbbox(ch, anchor="ls")
    if x1 <= x0 or y1 <= y0:
        return round(adv * 16), 0, 0, 0, 0, []
    pad = 2 * SS
    W, H = x1 - x0 + 2 * pad, y1 - y0 + 2 * pad
    img = Image.new("L", (W, H), 0)
    ox, oy = pad - x0, pad - y0                              # pen origin / baseline in big px
    ImageDraw.Draw(img).text((ox, oy), ch, font=big, fill=255, anchor="ls")
    a = np.asarray(img, np.float64)
    best = None
    for sy in range(-SS // 2, SS // 2):
        for sx in range(-SS // 2, SS // 2):
            # small row k covers big rows [oy - sy + SS*k, +SS): baseline at a row boundary
            gy0, gx0 = (oy - sy) % SS, (ox - sx) % SS
            sub = a[gy0:, gx0:]
            hh, ww = sub.shape[0] // SS * SS, sub.shape[1] // SS * SS
            small = sub[:hh, :ww].reshape(hh // SS, SS, ww // SS, SS).mean(axis=(1, 3))
            blur = float((small * (255 - small)).sum())
            if best is None or blur < best[0]:
                k0 = -((oy - sy - gy0) // SS)                # small-row index of the first row
                j0 = -((ox - sx - gx0) // SS)
                best = (blur, small, k0, j0)
    _, small, k0, j0 = best
    q = quantize(small)
    rows, cols = np.nonzero(q)
    if rows.size == 0:
        return round(adv * 16), 0, 0, 0, 0, []
    r0, r1, c0, c1 = rows.min(), rows.max(), cols.min(), cols.max()
    box = q[r0:r1 + 1, c0:c1 + 1]
    ofs_x = int(j0 + c0)
    ofs_y = -int(k0 + r1 + 1)                                # LVGL: box bottom above baseline
    vals = [min(15, (int(v) * 15 + 127) // 255) for v in box.flatten()]
    return round(adv * 16), int(box.shape[1]), int(box.shape[0]), ofs_x, ofs_y, vals


def pack4(vals):
    """Continuous 4bpp packing, high nibble first (as lv_font_fmt_txt reads it)."""
    out = bytearray()
    for i in range(0, len(vals), 2):
        hi = vals[i]
        lo = vals[i + 1] if i + 1 < len(vals) else 0
        out.append((hi << 4) | lo)
    return out


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("ttf")
    ap.add_argument("size", type=int)
    ap.add_argument("name")
    ap.add_argument("--range", default="32-126")
    ap.add_argument("--tabular", action="store_true",
                    help="equal-width digits (no sideways jiggle as values change)")
    a = ap.parse_args(argv)
    first, last = (int(v) for v in a.range.split("-"))

    font = ImageFont.truetype(a.ttf, a.size)
    ascent, descent = font.getmetrics()
    if a.size <= HINT_MAX_SIZE:                              # small sizes: grid-fit each glyph
        big = ImageFont.truetype(a.ttf, a.size * SS)
        render_glyph = lambda ch: render_fitted(big, ch)
    else:
        render_glyph = lambda ch: render(font, ch)
    bitmap, dsc = bytearray(), []
    digit_adv = max(render_glyph(d)[0] for d in "0123456789") if a.tabular else 0
    for cp in range(first, last + 1):
        adv, bw, bh, ox, oy, vals = render_glyph(chr(cp))
        if a.tabular and chr(cp).isdigit():                  # centre in the widest digit's advance
            ox += round((digit_adv - adv) / 16 / 2)
            adv = digit_adv
        dsc.append((len(bitmap), adv, bw, bh, ox, oy, chr(cp)))
        bitmap += pack4(vals)

    src = os.path.basename(a.ttf)
    print(f"/* {a.name}: {src} at {a.size} px, code points {first}-{last}, 4 bpp.")
    print(" * Generated by tools/mkfont.py; do not edit. Font licence: see sdk/fonts/README. */")
    print('#include "lvgl.h"\n')
    print("static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {")
    for i in range(0, len(bitmap), 16):
        print("    " + ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 16]) + ",")
    if not bitmap:
        print("    0x00,")
    print("};\n")
    print("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    print("    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id 0 reserved */,")
    for idx, adv, bw, bh, ox, oy, ch in dsc:
        label = ch if ch not in "\\*/" else " "
        print(f"    {{.bitmap_index = {idx}, .adv_w = {adv}, .box_w = {bw}, .box_h = {bh}, "
              f".ofs_x = {ox}, .ofs_y = {oy}}}, /* {label} */")
    print("};\n")
    print("static const lv_font_fmt_txt_cmap_t cmaps[] = {")
    print(f"    {{.range_start = {first}, .range_length = {last - first + 1}, .glyph_id_start = 1,")
    print("     .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0,")
    print("     .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY},")
    print("};\n")
    print("static const lv_font_fmt_txt_dsc_t font_dsc = {")
    print("    .glyph_bitmap = glyph_bitmap, .glyph_dsc = glyph_dsc, .cmaps = cmaps,")
    print("    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = 1, .bpp = 4,")
    print("    .kern_classes = 0, .bitmap_format = 0,")
    print("};\n")
    print(f"const lv_font_t {a.name} = {{")
    print("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    print("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    print(f"    .line_height = {ascent + descent},")
    print(f"    .base_line = {descent},")
    print("    .subpx = LV_FONT_SUBPX_NONE,")
    print("    .underline_position = -1,")
    print("    .underline_thickness = 1,")
    print("    .dsc = &font_dsc,")
    print("};")


if __name__ == "__main__":
    main()
