#!/usr/bin/env python3
"""vgafont.py — TEXT tool for the naos VGA 8x16 font (B3 preparation).

In VGA text mode, each character is an 8x16 grid stored in the character
generator. A font = 256 glyphs x 16 bytes = 4096 bytes.
  - glyph N   -> offset N*16
  - 1 byte    -> 1 row (8 pixels), from top to bottom
  - bit 7 (0x80) = LEFTMOST pixel
This 4096-byte block is what B3 will write into the VGA to replace CP437.

Subcommands:
  build  <out.bin> --ttf F   Build the font (256 CP437 glyphs) from a TTF (Pillow required).
  show   <font.bin> CHAR     Display a glyph in ASCII (#/.) + its 16 bytes. CHAR = 'A', 0x41 or 65.
  rough  <in.bin> <out.bin>  Apply the "rough" effect (scanlines + noise). NB: illegible at 8x16 (demo).
  export <font.bin> [--asm|--c]  Output the font as a NASM (db) or C array, ready to embed in B3.

Get the source TTF font: pack "Ultimate Oldschool PC Font Pack" (int10h.org,
CC BY-SA) -> file "Px (pixel outline)/PxPlus_IBM_VGA_8x16.ttf".
"""
import sys, argparse, os

GLYPHS, ROWS, WIDTH = 256, 16, 8
SIZE = GLYPHS * ROWS  # 4096


def load(path):
    d = open(path, "rb").read()
    if len(d) != SIZE:
        sys.exit(f"unexpected size: {len(d)} bytes (expected {SIZE})")
    return bytearray(d)


def code_of(s):
    """'A' | 0x41 | 65 -> integer 0..255."""
    if len(s) == 1 and not s.isdigit():
        return s.encode("cp437")[0]      # literal character -> CP437 byte
    return int(s, 0)                     # otherwise: 0x41, 65, 0o101…


def cmd_build(a):
    from PIL import Image, ImageFont, ImageDraw           # dev dependency only
    font = ImageFont.truetype(a.ttf, ROWS)
    out = bytearray(SIZE)
    for i in range(GLYPHS):
        ch = bytes([i]).decode("cp437")                   # CP437 byte -> Unicode character
        img = Image.new("L", (WIDTH, ROWS), 0)
        ImageDraw.Draw(img).text((0, 0), ch, fill=255, font=font)
        px = img.load()
        for y in range(ROWS):
            b = 0
            for x in range(WIDTH):
                b = (b << 1) | (1 if px[x, y] > 110 else 0)
            out[i * ROWS + y] = b
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    open(a.out, "wb").write(out)
    print(f"wrote {a.out} ({len(out)} bytes, {GLYPHS} glyphs 8x16)")


def cmd_show(a):
    d = load(a.font); c = code_of(a.char); g = d[c * ROWS:(c + 1) * ROWS]
    try: u = bytes([c]).decode("cp437")
    except Exception: u = "?"
    print(f"glyph 0x{c:02X} (CP437 = {u!r})  16 bytes: {' '.join('%02X' % r for r in g)}")
    for r in g:
        print("  " + "".join("#" if (r >> (7 - x)) & 1 else "." for x in range(WIDTH)))


def cmd_rough(a):
    import random
    random.seed(1234)
    d = load(a.inp)
    for i in range(GLYPHS):
        for y in range(ROWS):
            o = i * ROWS + y
            if y % 3 == 2 or random.random() < 0.10:       # 1 row/3 + ~10% noise
                d[o] = 0
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    open(a.out, "wb").write(d)
    print(f"wrote {a.out} (rough — reminder: 8x16 too small to stay legible)")


def cmd_export(a):
    d = load(a.font)
    if a.fmt == "asm":
        print("; naos VGA 8x16 font — 256 glyphs x 16 bytes")
        print("vga_font_8x16:")
        for i in range(GLYPHS):
            g = d[i * ROWS:(i + 1) * ROWS]
            print(f"    db {','.join('0x%02X' % b for b in g)}   ; 0x{i:02X}")
    else:
        print("/* naos VGA 8x16 font — 256 glyphs x 16 bytes */")
        print("const unsigned char vga_font_8x16[4096] = {")
        for i in range(GLYPHS):
            g = d[i * ROWS:(i + 1) * ROWS]
            print("    " + ",".join("0x%02X" % b for b in g) + f",   /* 0x{i:02X} */")
        print("};")


p = argparse.ArgumentParser(description="naos VGA 8x16 font — fetch / inspect / modify / export.")
sub = p.add_subparsers(dest="cmd", required=True)
b = sub.add_parser("build"); b.add_argument("out"); b.add_argument("--ttf", required=True); b.set_defaults(fn=cmd_build)
s = sub.add_parser("show"); s.add_argument("font"); s.add_argument("char"); s.set_defaults(fn=cmd_show)
r = sub.add_parser("rough"); r.add_argument("inp"); r.add_argument("out"); r.set_defaults(fn=cmd_rough)
e = sub.add_parser("export"); e.add_argument("font"); e.add_argument("--asm", dest="fmt", action="store_const", const="asm", default="c"); e.add_argument("--c", dest="fmt", action="store_const", const="c"); e.set_defaults(fn=cmd_export)
args = p.parse_args(); args.fn(args)
