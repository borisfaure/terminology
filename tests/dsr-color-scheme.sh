#!/bin/sh

# DSR CSI ? 996 n, Contour's colour scheme brightness query.
#
# Expected write buffer:
#   \e[?997;2n  fake BG 131/132/133, BT.601 0.517: light
#   \e[?997;1n  #000000: dark
#   \e[?997;2n  rgb:ff/ff/ff with an ST terminator: light
#   \e[?997;2n  #808080: 128000, light
#   \e[?997;1n  #7f7f7f: 127000, dark
#   \e[?997;1n  #0000ff: 29070, dark
#   \e[?997;2n  #ffff00: 225930, light
#   \e[?997;2n  ?996;5n: extra parameters are ignored, BG is still #ffff00
#   (nothing)   CSI 996 n without '?': not a private DSR, no reply
#   \e[?997;2n  DECSTR restores the scheme background: light again

# fake BG is 131/132/133: BT.601 0.517, light
printf '\033[?996n'
# black: dark
printf '\033]11;#000000\007'
printf '\033[?996n'
# white with an ST terminator: light
printf '\033]11;rgb:ff/ff/ff\033\\'
printf '\033[?996n'
# boundary: 0x80 grey is 128000 -> light, 0x7f grey is 127000 -> dark
printf '\033]11;#808080\007'
printf '\033[?996n'
printf '\033]11;#7f7f7f\007'
printf '\033[?996n'
# saturated blue is dark, saturated yellow is light
printf '\033]11;#0000ff\007'
printf '\033[?996n'
printf '\033]11;#ffff00\007'
printf '\033[?996n'
# extra parameters are ignored
printf '\033[?996;5n'
# not private: no reply
printf '\033[996n'
# DECSTR restores the scheme background, so light again
printf '\033[!p'
printf '\033[?996n'
