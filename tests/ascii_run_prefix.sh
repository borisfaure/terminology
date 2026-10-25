#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# plain ASCII followed by non-ASCII, then ASCII again, in a single run
printf '\033[1;1H'
printf 'abc\303\251def\342\202\254ghi'

# ASCII, DEL (dropped, ends the run), ASCII
printf '\033[2;1H'
printf 'abc\177def'

# ASCII, C1 CSI (U+009B) with a parameter, ASCII
printf '\033[3;1H'
printf 'abc\302\2331;31mdef'

# a run wider than the screen wraps onto the next line
printf '\033[5;1H'
printf '0123456789012345678901234567890123456789'
printf '0123456789012345678901234567890123456789'
printf '0123456789012345678901234567890123456789'
printf '\303\251tail'

# a run stopped by the right margin, then continued after the non-ASCII
printf '\033[?69h\033[10;20s'
printf '\033[8;10H'
printf 'abcdefghijklmnopqrstuvwxyz\303\251ABCDEFGHIJKLMNOP'
printf '\033[?69l'

# DEC special graphics: an ASCII run translated, then one back in ASCII
printf '\033[13;1H'
printf '\033(0lqqqk abc\033(Bxyz lqk'

# SO/SI shift into G1 (DEC special graphics) and back
printf '\033[14;1H'
printf '\033)0abc\016lqqqk abc\017xyz lqk'

# insert mode: a run pushes the existing text right, then a plain run
printf '\033[15;1H'
printf 'EXISTING'
printf '\033[15;1H\033[4h'
printf 'abcdef'
printf '\033[4l'
printf 'ghi'

# autowrap off near the right edge: the run piles up on the last column
printf '\033[?7l\033[16;75H'
printf 'abcdefghijkl'
printf '\033[?7h'
printf 'm'

# a run starting exactly at the right margin, then one beyond it
printf '\033[?69h\033[1;20s'
printf '\033[17;20H'
printf 'abcdef'
printf '\033[19;30H'
printf 'abcdef'

# a run ending exactly at the right margin, then more ASCII
printf '\033[20;10H'
printf 'abcdefghijk'
printf 'lmnopqr'
printf '\033[?69l'

# a run followed by U+0336 (combining strike), then more ASCII
printf '\033[22;1H'
printf 'abc\314\266def'

# a run ending right before U+2733 U+FE0F
printf '\033[23;1H'
printf 'abc\342\234\263\357\270\217def'

# a NUL inside a run
printf '\033[24;1H'
printf 'abc\000def'
