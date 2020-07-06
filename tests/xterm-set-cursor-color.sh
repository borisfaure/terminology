#!/bin/sh

# char width: 7
# char height: 15

# set color
printf '\033[0;31;3m'

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# set color
printf '\033[0m'

# set cursor color
printf '\033]12;#ff00ff\007'

# reset cursor color
printf '\033]112\007'
