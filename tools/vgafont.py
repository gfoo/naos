#!/usr/bin/env python3
"""vgafont.py — outil TEXTE pour la police VGA 8x16 de naos (préparation B3).

En mode texte VGA, chaque caractère est une grille 8x16 stockée dans le
générateur de caractères. Une police = 256 glyphes x 16 octets = 4096 octets.
  - glyphe N  -> offset N*16
  - 1 octet   -> 1 ligne (8 pixels), du haut vers le bas
  - bit 7 (0x80) = pixel le plus À GAUCHE
C'est ce bloc de 4096 octets que B3 écrira dans la VGA pour remplacer CP437.

Sous-commandes :
  build  <out.bin> --ttf F   Fabrique la police (256 glyphes CP437) depuis un TTF (Pillow requis).
  show   <font.bin> CHAR     Affiche un glyphe en ASCII (#/.) + ses 16 octets. CHAR = 'A', 0x41 ou 65.
  rough  <in.bin> <out.bin>  Applique l'effet "rough" (scanlines + bruit). NB: illisible à 8x16 (démo).
  export <font.bin> [--asm|--c]  Sort la police en tableau NASM (db) ou C, prêt à embarquer en B3.

Récupérer la police TTF source : pack « Ultimate Oldschool PC Font Pack » (int10h.org,
CC BY-SA) → fichier « Px (pixel outline)/PxPlus_IBM_VGA_8x16.ttf ».
"""
import sys, argparse, os

GLYPHS, ROWS, WIDTH = 256, 16, 8
SIZE = GLYPHS * ROWS  # 4096


def load(path):
    d = open(path, "rb").read()
    if len(d) != SIZE:
        sys.exit(f"taille inattendue: {len(d)} octets (attendu {SIZE})")
    return bytearray(d)


def code_of(s):
    """'A' | 0x41 | 65 -> entier 0..255."""
    if len(s) == 1 and not s.isdigit():
        return s.encode("cp437")[0]      # caractère littéral -> octet CP437
    return int(s, 0)                     # sinon: 0x41, 65, 0o101…


def cmd_build(a):
    from PIL import Image, ImageFont, ImageDraw           # dépendance dev seulement
    font = ImageFont.truetype(a.ttf, ROWS)
    out = bytearray(SIZE)
    for i in range(GLYPHS):
        ch = bytes([i]).decode("cp437")                   # octet CP437 -> caractère Unicode
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
    print(f"écrit {a.out} ({len(out)} octets, {GLYPHS} glyphes 8x16)")


def cmd_show(a):
    d = load(a.font); c = code_of(a.char); g = d[c * ROWS:(c + 1) * ROWS]
    try: u = bytes([c]).decode("cp437")
    except Exception: u = "?"
    print(f"glyphe 0x{c:02X} (CP437 = {u!r})  16 octets: {' '.join('%02X' % r for r in g)}")
    for r in g:
        print("  " + "".join("#" if (r >> (7 - x)) & 1 else "." for x in range(WIDTH)))


def cmd_rough(a):
    import random
    random.seed(1234)
    d = load(a.inp)
    for i in range(GLYPHS):
        for y in range(ROWS):
            o = i * ROWS + y
            if y % 3 == 2 or random.random() < 0.10:       # 1 ligne/3 + ~10% de bruit
                d[o] = 0
    os.makedirs(os.path.dirname(a.out) or ".", exist_ok=True)
    open(a.out, "wb").write(d)
    print(f"écrit {a.out} (rough — rappel: 8x16 trop petit pour rester lisible)")


def cmd_export(a):
    d = load(a.font)
    if a.fmt == "asm":
        print("; police VGA 8x16 naos — 256 glyphes x 16 octets")
        print("vga_font_8x16:")
        for i in range(GLYPHS):
            g = d[i * ROWS:(i + 1) * ROWS]
            print(f"    db {','.join('0x%02X' % b for b in g)}   ; 0x{i:02X}")
    else:
        print("/* police VGA 8x16 naos — 256 glyphes x 16 octets */")
        print("const unsigned char vga_font_8x16[4096] = {")
        for i in range(GLYPHS):
            g = d[i * ROWS:(i + 1) * ROWS]
            print("    " + ",".join("0x%02X" % b for b in g) + f",   /* 0x{i:02X} */")
        print("};")


p = argparse.ArgumentParser(description="Police VGA 8x16 naos — récupérer / consulter / modifier / exporter.")
sub = p.add_subparsers(dest="cmd", required=True)
b = sub.add_parser("build"); b.add_argument("out"); b.add_argument("--ttf", required=True); b.set_defaults(fn=cmd_build)
s = sub.add_parser("show"); s.add_argument("font"); s.add_argument("char"); s.set_defaults(fn=cmd_show)
r = sub.add_parser("rough"); r.add_argument("inp"); r.add_argument("out"); r.set_defaults(fn=cmd_rough)
e = sub.add_parser("export"); e.add_argument("font"); e.add_argument("--asm", dest="fmt", action="store_const", const="asm", default="c"); e.add_argument("--c", dest="fmt", action="store_const", const="c"); e.set_defaults(fn=cmd_export)
args = p.parse_args(); args.fn(args)
