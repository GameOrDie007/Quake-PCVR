#!/usr/bin/env python
"""
Build the re-release episodes' message text from classic Quake's own progs.

Dimension of the Past, Dimension of the Machine and Dawn of the Machine are
built on the 2021 re-release's QuakeC, where every player-facing message was
replaced by a localisation token - "$qc_need_gold_key" where the original had
"You need the gold key". The KEX engine resolves those against a string table
it keeps internally; that table is not in the game data, so this engine has
nothing to resolve them with.

Their QuakeC is the original with the strings swapped, so the original text is
sitting in id1's own progs.dat: a token's words appear verbatim in the classic
string it replaced. This matches them up and writes the result next to the
paks, where the engine reads it.

Both progs come from the owner's own install, and the output goes into his own
install. Nothing is copied into this repository, as with the paks and the menu
artwork.

Usage: make-qc-strings.py <install dir>
"""

import io
import os
import re
import struct
import sys


def pak_entries(path):
    with open(path, "rb") as f:
        magic, off, length = struct.unpack("<4sii", f.read(12))
        f.seek(off)
        out = {}
        for _ in range(length // 64):
            e = f.read(64)
            name = e[:56].split(b"\0")[0].decode("latin1")
            o, s = struct.unpack("<ii", e[56:64])
            out[name] = (o, s)
        f.seek(0)
        return out, open(path, "rb")


def progs_strings(pakpath):
    """Every string in a pak's progs.dat."""
    entries, f = pak_entries(pakpath)
    if "progs.dat" not in entries:
        return []
    o, s = entries["progs.dat"]
    f.seek(o)
    b = f.read(s)
    f.close()
    hdr = struct.unpack("<12i", b[8:56])
    ofs_str, n_str = hdr[8], hdr[9]
    return [x.decode("latin1") for x in b[ofs_str:ofs_str + n_str].split(b"\0")]


# Tokens are lowercase words joined by underscores, after a qc_ or <mod>_qc_
# prefix. "need gold key" has to appear in "You need the gold key".
def token_words(tok):
    body = tok.lstrip("$")
    body = re.sub(r"^[a-z0-9]*_?qc_", "", body)
    return [w for w in body.split("_") if w]


def match(words, candidates):
    best = None
    for c in candidates:
        cl = c.lower()
        if all(re.search(r"\b%s" % re.escape(w), cl) for w in words):
            # The shortest string containing every word is the tightest fit.
            if best is None or len(c) < len(best):
                best = c
    return best


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: make-qc-strings.py <install dir>\n")
        return 1
    dest = sys.argv[1]

    classic = [s for s in progs_strings(os.path.join(dest, "id1", "pak0.pak"))
               if s and " " in s and len(s) < 80 and not s.endswith(".qc")]

    tokens = set()
    for game in ("dopa", "mg1", "mg3"):
        p = os.path.join(dest, game, "pak0.pak")
        if os.path.isfile(p):
            for s in progs_strings(p):
                if s.startswith("$"):
                    tokens.add(s)

    if not tokens:
        print("  no re-release episodes installed, nothing to do")
        return 0

    out = []
    matched = 0
    for tok in sorted(tokens):
        words = token_words(tok)
        text = match(words, classic) if words else None
        if text is None:
            # No classic equivalent - the episodes' own new messages. The token
            # spelled out is a good deal better than nothing on screen.
            text = " ".join(words).capitalize()
        else:
            matched += 1
        out.append((tok, text))

    path = os.path.join(dest, "id1", "qc_strings.txt")
    with io.open(path, "w", encoding="latin1", newline="\n") as f:
        f.write("// Message text for the re-release episodes, built by\n")
        f.write("// tools/make-qc-strings.py from this install's own progs.dat files.\n")
        for tok, text in out:
            f.write("%s\t%s\n" % (tok, text))

    print("  %d tokens, %d matched to classic Quake text -> %s"
          % (len(out), matched, os.path.basename(path)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
