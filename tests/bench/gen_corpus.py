#!/usr/bin/env python3
"""Generate deterministic benchmark corpora for tybench.

Four workloads, chosen so that a change to one part of the intake path shows up
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


GENERATORS = {
    "plain-ascii": gen_plain_ascii,
    "scroll": gen_scroll,
    "sgr": gen_sgr,
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
