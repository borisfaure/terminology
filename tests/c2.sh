#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'

# set C1
printf '\x1b\x22\x43'

# move
printf '\xc2\x1b[4;4H#'
