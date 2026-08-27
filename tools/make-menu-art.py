#!/usr/bin/env python
"""
Render the game select page's entries as artwork, in the 2021 re-release's
own menu font.

Quake's ornate menu lettering exists only as pre-rendered pictures - "NEW
GAME", "LOAD", "SAVE" and so on are images in gfx/, not a font - so arbitrary
names like "Dimension of the Machine" cannot be drawn in it. The re-release
does have that alphabet as a real font, and its own expansion menu is drawn
with it, which is the look being matched here.

The font is read from the owner's own Quake install and the pictures are
written into his own install. Nothing is copied into this repository, exactly
as the packaging script treats his paks.

  QuakeEX.kpf is a zip. fonts/qfont.png is the atlas and fonts/qfont.kfont is
  a plain text table of "codepoint x y width height offset" - one line per
  glyph, all 28 tall.

Usage: make-menu-art.py <install dir> [quake dir]
"""

import io
import os
import struct
import sys
import zipfile

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("make-menu-art: no PIL, skipping menu artwork\n")
    sys.exit(0)

# Must match the gameselect_games table in menu.c.
NAMES = [
    ("quake",    "Quake"),
    ("hipnotic", "Scourge of Armagon"),
    ("rogue",    "Dissolution of Eternity"),
    ("dopa",     "Dimension of the Past"),
    ("mg1",      "Dimension of the Machine"),
    ("mg3",      "Dawn of the Machine"),
]

TRACKING = 1  # pixels between glyphs, on top of each glyph's own width

# The atlas glyphs are a dark bronze, which reads well on the re-release's own
# pale menu but disappears against a dimmed Quake level - and worse through a
# headset. Lifted so they carry at the edge of vision; the hue is untouched.
BRIGHTEN = 1.7


def load_font(kpf_path):
    z = zipfile.ZipFile(kpf_path)
    atlas = Image.open(io.BytesIO(z.read("fonts/qfont.png"))).convert("RGBA")
    glyphs = {}
    for line in z.read("fonts/qfont.kfont").decode("latin1").splitlines():
        parts = line.strip().split()
        if len(parts) == 6 and parts[0].isdigit():
            cp, x, y, w, h, off = (int(p) for p in parts)
            glyphs[cp] = (x, y, w, h, off)
    return atlas, glyphs


def render(atlas, glyphs, text):
    used = [glyphs[ord(c)] for c in text if ord(c) in glyphs]
    if not used:
        return None

    width = sum(g[2] for g in used) + TRACKING * (len(used) - 1)
    height = max(g[3] + g[4] for g in used)
    out = Image.new("RGBA", (width, height), (0, 0, 0, 0))

    x = 0
    for (gx, gy, gw, gh, off) in used:
        out.paste(atlas.crop((gx, gy, gx + gw, gy + gh)), (x, off))
        x += gw + TRACKING

    px = out.load()
    for yy in range(out.size[1]):
        for xx in range(out.size[0]):
            r, g, b, a = px[xx, yy]
            if a:
                px[xx, yy] = (min(255, int(r * BRIGHTEN)),
                              min(255, int(g * BRIGHTEN)),
                              min(255, int(b * BRIGHTEN)), a)

    return out


def write_tga(im, path):
    """32 bit uncompressed BGRA, bottom-up - the plainest thing DarkPlaces reads."""
    w, h = im.size
    px = im.load()
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, 32, 8)
    rows = []
    for y in range(h - 1, -1, -1):
        row = bytearray()
        for x in range(w):
            r, g, b, a = px[x, y]
            row += bytes((b, g, r, a))
        rows.append(bytes(row))
    with open(path, "wb") as f:
        f.write(header)
        f.write(b"".join(rows))


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: make-menu-art.py <install dir> [quake dir]\n")
        return 1

    dest = sys.argv[1]
    quakedir = sys.argv[2] if len(sys.argv) > 2 else os.environ.get(
        "QQ_QUAKEDIR", "C:/Program Files (x86)/Steam/steamapps/common/Quake")

    kpf = os.path.join(quakedir, "rerelease", "QuakeEX.kpf")
    if not os.path.isfile(kpf):
        sys.stderr.write("make-menu-art: no %s, skipping menu artwork\n" % kpf)
        return 0

    atlas, glyphs = load_font(kpf)

    gfxdir = os.path.join(dest, "id1", "gfx")
    if not os.path.isdir(gfxdir):
        os.makedirs(gfxdir)

    for key, text in NAMES:
        im = render(atlas, glyphs, text)
        if im is None:
            continue
        write_tga(im, os.path.join(gfxdir, "gs_%s.tga" % key))
        print("  %-24s %dx%d" % (text, im.size[0], im.size[1]))

    return 0


if __name__ == "__main__":
    sys.exit(main())
