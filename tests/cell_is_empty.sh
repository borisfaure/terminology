#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 8;8
printf '\033[8;8H#'
#move to 8;40
printf '\033[8;40H#'
#switch to altbuf
printf '\033[?1049h'

# change colors
printf '\033[m\033[37m\033[40m'
# move to 0;0
printf '\033[H'
# clear screen
printf '\033[2J'
# change colors
printf '\033[37m\033[40m\033[97m\033[40m'
# move
printf '\033[3;2H'
# text
printf '**\o/**'
