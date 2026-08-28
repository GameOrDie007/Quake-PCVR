#!/usr/bin/env python
"""
Build the re-release episodes' message text.

Dimension of the Past, Dimension of the Machine and Dawn of the Machine run on
the 2021 re-release's QuakeC, where every player-facing message is a
localisation token - "$qc_need_gold_key" where the original had "You need the
gold key", "$mg3_map1_upgrade_intro" for one of the episode's own lines. Their
engine resolves those against a table it keeps internally.

Two places have the wording, both inside the owner's own game data:

  * fgd/*.fgd in each episode's pak. These are level editor definition files,
    and they carry the episode's own messages verbatim, as
    "$token" : "the text". This is the authentic source and covers the
    episodes' new writing, which exists nowhere else.

  * id1's own progs.dat, for the classic messages. Their QuakeC is the
    original with the strings swapped, so a token's words appear verbatim in
    the string it replaced - "need gold key" in "You need the gold key".

Tokens are collected from the progs and from every map's entity lump, since
plenty of them are entity "message" keys rather than QuakeC constants.

Everything is read from his install and written back into it, as with the paks
and the menu artwork. Nothing is copied into this repository.

Usage: make-qc-strings.py <install dir>
"""

import io
import os
import re
import struct
import sys

GAMES = ("id1", "hipnotic", "rogue", "dopa", "mg1", "mg3")

TOKEN = re.compile(r"\$[A-Za-z0-9_]+")
# "$token" : "text", with \" allowed inside the text
FGDPAIR = re.compile(r'"(\$[A-Za-z0-9_]+)"\s*:\s*"((?:[^"\\]|\\.)*)"')


def pak_files(path):
    """Yield (name, bytes) for every file in a pak."""
    with open(path, "rb") as f:
        magic, off, length = struct.unpack("<4sii", f.read(12))
        f.seek(off)
        entries = []
        for _ in range(length // 64):
            e = f.read(64)
            name = e[:56].split(b"\0")[0].decode("latin1")
            o, s = struct.unpack("<ii", e[56:64])
            entries.append((name, o, s))
        for name, o, s in entries:
            f.seek(o)
            yield name, f.read(s)


def progs_strings(data):
    hdr = struct.unpack("<12i", data[8:56])
    ofs_str, n_str = hdr[8], hdr[9]
    return [x.decode("latin1") for x in data[ofs_str:ofs_str + n_str].split(b"\0")]


def bsp_entities(data):
    if len(data) < 12:
        return ""
    eo, el = struct.unpack("<ii", data[4:12])
    if eo < 0 or el < 0 or eo + el > len(data):
        return ""
    return data[eo:eo + el].decode("latin1", "replace")


def token_words(tok):
    body = re.sub(r"^[a-z0-9]*_?qc_", "", tok.lstrip("$"))
    return [w for w in body.split("_") if w]


# Where neither source has the wording - the episodes' own lines whose text
# lives only inside their engine - the token still says what the message was
# about. Dropping the leading context, which names the mod and the map rather
# than saying anything, leaves something a player can read.
CONTEXT = re.compile(r"^(qc|m|map|hub|mg[0-9]|dopa|hip|hipnotic|rogue|map[0-9]+[a-z]?)$")


def spell_out(words):
    while len(words) > 1 and CONTEXT.match(words[0]):
        words = words[1:]
    return " ".join(words).capitalize()


def match_classic(words, candidates):
    best = None
    for c in candidates:
        cl = c.lower()
        if all(re.search(r"\b%s" % re.escape(w), cl) for w in words):
            if best is None or len(c) < len(best):
                best = c
    return best


def main():
    if len(sys.argv) < 2:
        sys.stderr.write("usage: make-qc-strings.py <install dir>\n")
        return 1
    dest = sys.argv[1]

    tokens = set()
    fgd = {}
    classic = []

    for game in GAMES:
        pak = os.path.join(dest, game, "pak0.pak")
        if not os.path.isfile(pak):
            continue
        for name, data in pak_files(pak):
            lname = name.lower()
            if lname.endswith(".fgd"):
                for m in FGDPAIR.finditer(data.decode("latin1", "replace")):
                    fgd.setdefault(m.group(1), m.group(2))
            elif lname == "progs.dat":
                for s in progs_strings(data):
                    if s.startswith("$"):
                        tokens.add(s)
                    elif game == "id1" and " " in s and len(s) < 80:
                        classic.append(s)
            elif lname.endswith(".bsp"):
                tokens.update(TOKEN.findall(bsp_entities(data)))

    tokens.update(fgd.keys())
    if not tokens:
        print("  no re-release episodes installed, nothing to do")
        return 0

    rows, from_fgd, from_classic = [], 0, 0
    for tok in sorted(tokens):
        if tok in fgd:
            text = fgd[tok]
            from_fgd += 1
        else:
            words = token_words(tok)
            text = match_classic(words, classic) if words else None
            if text:
                from_classic += 1
            else:
                # Neither source has it. The token spelled out at least shows
                # that the game said something.
                text = spell_out(words)
        # One entry per line, so a message's own line breaks stay escaped and
        # the engine turns them back into newlines as it reads them.
        text = text.replace("\r", "").replace("\n", "\\n")
        rows.append((tok, text))

    path = os.path.join(dest, "id1", "qc_strings.txt")
    with io.open(path, "w", encoding="latin1", newline="\n") as f:
        f.write("// Message text for the re-release episodes, built by\n")
        f.write("// tools/make-qc-strings.py from this install's own game data.\n")
        for tok, text in rows:
            f.write("%s\t%s\n" % (tok, text))

    print("  %d messages: %d from their own .fgd, %d recovered from Quake, %d unknown"
          % (len(rows), from_fgd, from_classic, len(rows) - from_fgd - from_classic))
    return 0


if __name__ == "__main__":
    sys.exit(main())
