#!/bin/sh

# char width: 7
# char height: 15

# set color
printf '\033[0;31;3m'

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

printf 'The purpose of computing is insight, not numbers.\r\n'
printf 'Richard Hamming\r\n'

# set color
printf '\033[0m'

# X11 colour names on the foreground, then query
printf '\033]10;red\007'
printf '\033]10;?\007'
# case-insensitive, spaces allowed
printf '\033]10;LIGHTSKYBLUE\007'
printf '\033]10;?\007'
printf '\033]10;navy blue\007'
printf '\033]10;?\007'
# the gray ramp
printf '\033]10;gray42\007'
printf '\033]10;?\007'

# the background and the cursor too
printf '\033]11;SeaGreen\007'
printf '\033]11;?\007'
printf '\033]12;cornflower blue\007'
printf '\033]12;?\007'

# not names: the colour stays as it was
printf '\033]10;notacolor\007'
printf '\033]10;?\007'
# "default" resets to the configured colour
printf '\033]10;default\007'
printf '\033]10;?\007'

# reset the colors changed above
printf '\033]110\007\033]111\007\033]112\007'
