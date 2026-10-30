#!/bin/sh

# OSC 10, 11 and 12 colour sets were dropped when the string was
# terminated with ESC \ instead of BEL: the length passed to the
# colour parsers counted the two-character terminator.

# foreground
printf '\033]10;#ff0000\033\\'
printf '\033]10;?\007'
# background, rgb form
printf '\033]11;rgb:00/ff/00\033\\'
printf '\033]11;?\007'
# cursor, rgbi form
printf '\033]12;rgbi:0/0/1.0\033\\'
printf '\033]12;?\007'
# BEL-terminated sets still work
printf '\033]10;#00ff00\007'
printf '\033]10;?\007'
# an invalid colour is still rejected with ESC \ as the terminator
printf '\033]10;#ff00\033\\'
printf '\033]10;?\007'
# the 9-digit # form: two digits per channel, the third checked but dropped
printf '\033]11;#123456789\033\\'
printf '\033]11;?\007'
# a one-digit rgb: component scales to a full byte
printf '\033]12;rgb:f/0/8\033\\'
printf '\033]12;?\007'
# under #, a nibble is the most significant bits, not scaled
printf '\033]12;#fff\033\\'
printf '\033]12;?\007'
