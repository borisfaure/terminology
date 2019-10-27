#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

# move to 2; 2
printf '\033[2;2H'

# Set string to select
printf 'a\011b\011'

# mouse down to start selection
printf '\033}td;0;24;1;0;0\0'

# mouse move
printf '\033}tm;78;24\0'

# mouse up
printf '\033}tu;78;24;1;0;0\0'

# force render
printf '\033}tr\0'

# selection is 'EEEEE'
printf '\033}ts a\011b\n\0'


