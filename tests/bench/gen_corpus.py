#!/usr/bin/env python3
"""Generate deterministic benchmark corpora for tybench.

Eleven workloads, chosen so that a change to one part of the intake path shows up
in one column rather than being averaged away:

  plain-ascii  long lines of printable ASCII, almost no escapes. The pure
               text-append path -- what an ASCII fast path targets.
  scroll       short lines, so the cost is dominated by line feeds, wrapping and
               backlog pushes rather than by cell writes.
  sgr          colour-saturated output. Exercises the CSI parser and its
               parameter scanning instead of the text path.
  unicode      CJK, emoji, combining marks and accented Latin. Exercises
               multibyte decode, double-width handling, and the paths a
               byte-space fast path has to bail out of correctly.
  strings      long OSC titles, sixel DCS, kitty graphics APC and long SGR
               parameter lists. Exercises sequences that span many reads,
               to be benchmarked with a small --chunk.
  rep          a character then REP (CSI b), as curses draws rules and fills
               when terminfo has rep. Exercises the REP expansion.
  fill         DECFRA rectangles under default and set colours, and DECALN.
               Exercises the rectangle and screen fills.
  insert       ICH (CSI @) on a row, as editors open room for typed text, and
               DECIC (CSI ' }) now and then. Exercises the in-row shifts.
  insert-mode  words and REPs typed under IRM (CSI 4 h), as when terminfo
               has smir, with DCH and DECBI/DECFI now and then. Exercises
               the row shift every character pays in insert mode.
  long-lines   lines kilobytes long, as minified JSON or one-line logs, with
               an accented word now and then. Exercises the ASCII runs that
               follow a non-ASCII character within one read.
  hyperlink    file paths behind OSC 8 hyperlinks, as ls --hyperlink prints,
               scrolling in a scroll region, as under a status line.
               Exercises the link refcounts kept when cells are copied.

Output is byte-identical across runs (fixed seed, no clock, no locale) so that
numbers from different days are comparable.
"""

import argparse
import os
import random

TARGET_DEFAULT = 4 * 1024 * 1024

WORDS = (
    "terminal escape sequence parser buffer cursor render glyph column row "
    "codepoint attribute palette scrollback viewport selection backlog cell "
    "unicode decode dispatch throughput latency kernel syscall pipeline vector"
).split()


def _rng(tag):
    # Per-corpus seed: adding a corpus cannot change the bytes of another.
    return random.Random("terminology-bench:" + tag)


def gen_plain_ascii(target):
    """Long lines of printable ASCII. Maximises consecutive printable runs."""
    rng = _rng("plain-ascii")
    out = bytearray()
    while len(out) < target:
        line = []
        width = 0
        # Aim well past the 80-column screen so wrapping is exercised too.
        while width < 100:
            w = rng.choice(WORDS)
            line.append(w)
            width += len(w) + 1
        out += (" ".join(line) + "\n").encode("ascii")
    return bytes(out[:target])


def gen_scroll(target):
    """Short lines: one newline every few bytes, so scrolling dominates."""
    rng = _rng("scroll")
    out = bytearray()
    n = 0
    while len(out) < target:
        out += ("%6d %s\n" % (n, rng.choice(WORDS))).encode("ascii")
        n += 1
    return bytes(out[:target])


def gen_sgr(target):
    """Colour-heavy output: an SGR sequence for nearly every short text run."""
    rng = _rng("sgr")
    out = bytearray()
    cut = 0
    while len(out) < target:
        style = rng.choice(
            [
                "\033[%dm" % rng.randint(30, 37),
                "\033[1;%dm" % rng.randint(30, 37),
                "\033[38;5;%dm" % rng.randint(0, 255),
                "\033[48;5;%dm" % rng.randint(0, 255),
                "\033[38;2;%d;%d;%dm"
                % (rng.randint(0, 255), rng.randint(0, 255), rng.randint(0, 255)),
                "\033[0m",
                "\033[1m",
                "\033[4m",
            ]
        )
        out += (style + rng.choice(WORDS)).encode("ascii")
        if rng.random() < 0.15:
            out += b"\033[0m\n"
        # Remember where a cut may fall. Truncating at an arbitrary byte leaves
        # half an escape sequence at the end, and since each pass restarts at
        # the top of the corpus, that half runs into the leading ESC [ of the
        # next pass -- an ESC [ [ the parser rightly complains about.
        if len(out) <= target:
            cut = len(out)
    # A target smaller than the first sequence still gets whole sequences.
    if cut == 0:
        cut = len(out)
    # End reset, so a pass cannot leave attributes set for the pass after it.
    return bytes(out[:cut]) + b"\033[0m\n"


def gen_unicode(target):
    """Mixed multibyte: 2-, 3- and 4-byte sequences plus combining marks."""
    rng = _rng("unicode")
    pools = [
        "éèêüñåøæ",   # 2-byte Latin-1
        "你好世界漢字日本",   # 3-byte CJK, wide
        "αβγδЖДЯш",   # 2-byte Greek/Cyrillic
        "\U0001f600\U0001f680\U0001f4a1\U0001f30d",           # 4-byte emoji
    ]
    out = bytearray()
    while len(out) < target:
        line = []
        for _ in range(rng.randint(8, 20)):
            pool = rng.choice(pools)
            chunk = "".join(rng.choice(pool) for _ in range(rng.randint(1, 6)))
            # Sprinkle combining acute accents onto some Latin runs.
            if pool is pools[0] and rng.random() < 0.3:
                chunk += "́"
            line.append(chunk)
        # Interleave ASCII so the corpus exercises transitions in and out of the
        # multibyte path rather than staying in one mode.
        line.append(rng.choice(WORDS))
        out += (" ".join(line) + "\n").encode("utf-8")
    # Never truncate mid-sequence: that would make the corpus itself invalid.
    data = bytes(out)
    # Whole lines are appended, so the buffer already ends on a complete
    # character; if the last one landed exactly on the target there is no byte
    # past it to inspect and nothing to trim.
    if target >= len(data):
        return data
    cut = target
    while cut > 0 and (data[cut] & 0xC0) == 0x80:
        cut -= 1
    return data[:cut]


def gen_strings(target):
    """Sequences hundreds to thousands of bytes long, between short lines."""
    rng = _rng("strings")
    sixel = "#0;2;0;0;0#1;2;100;0;0#0~~@@vv@@~~$#1??}}GG}}??-"
    base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
    out = bytearray()
    cut = 0
    while len(out) < target:
        kind = rng.randrange(4)
        if kind == 0:
            words = " ".join(rng.choice(WORDS) for _ in range(rng.randint(20, 250)))
            seq = "\033]2;%s\033\\" % words
        elif kind == 1:
            seq = "\033Pq%s\033\\" % (sixel * rng.randint(10, 60))
        elif kind == 2:
            payload = "".join(rng.choice(base64) for _ in range(rng.randint(1000, 4000)))
            seq = "\033_Ga=T,f=100;%s\033\\" % payload
        else:
            params = ";".join("38;5;%d" % rng.randint(0, 255)
                              for _ in range(rng.randint(10, 50)))
            seq = "\033[%sm" % params
        out += (seq + rng.choice(WORDS) + "\n").encode("ascii")
        # Whole sequences only, as in gen_sgr.
        if len(out) <= target:
            cut = len(out)
    if cut == 0:
        cut = len(out)
    return bytes(out[:cut]) + b"\033[0m\n"


def gen_rep(target):
    """Cursor moves, a character and a REP of it, between short words."""
    rng = _rng("rep")
    out = bytearray()
    cut = 0
    while len(out) < target:
        ch = rng.choice("-=_.#* \u2500\u2501\u2550\u2588")
        seq = "\033[%d;%dH%s\033[%db%s" % (rng.randint(1, 24), rng.randint(1, 10),
                                         ch, rng.randint(1, 79), rng.choice(WORDS))
        out += seq.encode("utf-8")
        # Whole sequences only, as in gen_sgr.
        if len(out) <= target:
            cut = len(out)
    if cut == 0:
        cut = len(out)
    return bytes(out[:cut])


def gen_fill(target):
    """DECFRA rectangles, some under an SGR colour, a DECALN now and then."""
    rng = _rng("fill")
    out = bytearray()
    cut = 0
    while len(out) < target:
        if rng.random() < 0.05:
            seq = "\033#8"
        else:
            top, left = rng.randint(1, 24), rng.randint(1, 80)
            sgr = rng.choice(["\033[0m", "\033[0;%dm" % rng.randint(31, 37),
                              "\033[0;48;5;%dm" % rng.randint(0, 255)])
            seq = "%s\033[%d;%d;%d;%d;%d$x" % (sgr, rng.randint(33, 126), top, left,
                                              rng.randint(top, 24),
                                              rng.randint(left, 80))
        out += (seq + rng.choice(WORDS)).encode("ascii")
        # Whole sequences only, as in gen_sgr.
        if len(out) <= target:
            cut = len(out)
    if cut == 0:
        cut = len(out)
    return bytes(out[:cut]) + b"\033[0m\n"


def gen_insert(target):
    """Cursor moves, an ICH or a DECIC of random count, then a word."""
    rng = _rng("insert")
    out = bytearray()
    cut = 0
    while len(out) < target:
        final = "'}" if rng.random() < 0.05 else "@"
        seq = "\033[%d;%dH\033[%d%s%s" % (rng.randint(1, 24), rng.randint(1, 80),
                                          rng.randint(1, 80), final,
                                          rng.choice(WORDS))
        out += seq.encode("ascii")
        # Whole sequences only, as in gen_sgr.
        if len(out) <= target:
            cut = len(out)
    if cut == 0:
        cut = len(out)
    return bytes(out[:cut])


def gen_insert_mode(target):
    """Cursor moves, then a word or a REP typed in insert mode."""
    rng = _rng("insert-mode")
    out = bytearray(b"\033[4h")
    cut = len(out)
    while len(out) < target:
        r = rng.random()
        if r < 0.05:
            seq = "\033[%d;1H\0336" % rng.randint(1, 24)
        elif r < 0.10:
            seq = "\033[%d;80H\0339" % rng.randint(1, 24)
        elif r < 0.20:
            seq = "\033[%d;%dH\033[%dP" % (rng.randint(1, 24), rng.randint(1, 80),
                                           rng.randint(1, 80))
        elif r < 0.40:
            seq = "\033[%d;%dH%s\033[%db" % (rng.randint(1, 24), rng.randint(1, 40),
                                             rng.choice("-=_.# "),
                                             rng.randint(1, 79))
        else:
            seq = "\033[%d;%dH%s " % (rng.randint(1, 24), rng.randint(1, 80),
                                     rng.choice(WORDS))
        out += seq.encode("ascii")
        # Whole sequences only, as in gen_sgr.
        if len(out) <= target:
            cut = len(out)
    return bytes(out[:cut]) + b"\033[4l"


def gen_long_lines(target):
    """Lines of 4 to 16 KiB of words, about one in 200 of them accented."""
    rng = _rng("long-lines")
    out = bytearray()
    while len(out) < target:
        line = []
        width = 0
        limit = rng.randint(4096, 16384)
        while width < limit:
            w = rng.choice(WORDS)
            if rng.random() < 0.005:
                w = rng.choice(["caf\u00e9", "na\u00efve", "\u00fcber", "se\u00f1or"])
            line.append(w)
            width += len(w) + 1
        out += (" ".join(line) + "\n").encode("utf-8")
    return bytes(out)


def gen_hyperlink(target):
    """Hyperlinked paths, one or two per line, in a 23-row scroll region."""
    rng = _rng("hyperlink")
    out = bytearray(b"\033[1;23r\033[23;1H")
    while len(out) < target:
        line = ""
        for _ in range(rng.randint(1, 2)):
            path = "/".join(rng.choice(WORDS) for _ in range(rng.randint(1, 4)))
            line += "  \033]8;;file://host/%s\033\\%s\033]8;;\033\\" % (path, path)
        out += (line + "\n").encode("ascii")
    return bytes(out) + b"\033[r"


GENERATORS = {
    "fill": gen_fill,
    "hyperlink": gen_hyperlink,
    "insert": gen_insert,
    "insert-mode": gen_insert_mode,
    "long-lines": gen_long_lines,
    "plain-ascii": gen_plain_ascii,
    "rep": gen_rep,
    "scroll": gen_scroll,
    "sgr": gen_sgr,
    "strings": gen_strings,
    "unicode": gen_unicode,
}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("outdir", help="directory to write corpora into")
    ap.add_argument("-s", "--size", type=int, default=TARGET_DEFAULT,
                    help="approximate bytes per corpus (default %d)" % TARGET_DEFAULT)
    ap.add_argument("-o", "--only", action="append", choices=sorted(GENERATORS),
                    help="generate only this corpus (repeatable)")
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    names = args.only if args.only else sorted(GENERATORS)

    for name in names:
        data = GENERATORS[name](args.size)
        path = os.path.join(args.outdir, name)
        # Skip the rewrite if content is already correct, so timestamps stay put
        # and build systems do not re-run downstream steps for nothing.
        if os.path.exists(path):
            with open(path, "rb") as f:
                if f.read() == data:
                    print("%-14s %9d bytes (unchanged)" % (name, len(data)))
                    continue
        with open(path, "wb") as f:
            f.write(data)
        print("%-14s %9d bytes" % (name, len(data)))


if __name__ == "__main__":
    main()
