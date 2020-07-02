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

printf 'The purpose of computing is insight, not numbers.\r\n'
printf 'Richard Hamming\r\n'

# valid colors
printf '\033]10;rgbi:1.0/0/1.0\007'
printf '\033]10;rgbi:1/0.0/1\007'
printf '\033]10;rgbi:1/0.0/1\007'

##
# invalid
##

printf '\033]10;rgbi:1.1/0/1\007'
printf '\033]10;rgbi:f/1.1/1\007'
printf '\033]10;rgbi:f/0/1.1\007'

printf '\033]10;rgbi:-0.1/0/1\007'
printf '\033]10;rgbi:1/-0.1/1\007'
printf '\033]10;rgbi:1/0/-0.1\007'

printf '\033]10;rgbi:+Inf/0/1\007'
printf '\033]10;rgbi:1/+Inf/0\007'
printf '\033]10;rgbi:1/0/+Inf\007'

printf '\033]10;rgbi:1.0|0.0/1.0\007'
printf '\033]10;rgbi:1.0/0.0|1.0\007'

printf '\033]10;rgbi:1.0/0.0/1.0/0.0\007'
