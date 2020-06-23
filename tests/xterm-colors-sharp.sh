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

# valid colors
# one nibble per color
printf '\033]10;#f0f\007'
# two nibble per color
printf '\033]10;#ff00ff\007'
# three nibble per color
printf '\033]10;#fff000fff\007'
# four nibble per color
printf '\033]10;#FFFF0000FFFF\007'

##
# invalid
##

# not hex
# one nibble per color
printf '\033]10;#g0f\007'
printf '\033]10;#fgf\007'
printf '\033]10;#f0g\007'
# two nibble per color
printf '\033]10;#gf00ff\007'
printf '\033]10;#fg00ff\007'
printf '\033]10;#ffg0ff\007'
printf '\033]10;#ff0gff\007'
printf '\033]10;#ff00gf\007'
printf '\033]10;#ff00fg\007'
# three nibble per color
printf '\033]10;#gff000fff\007'
printf '\033]10;#fgf000fff\007'
printf '\033]10;#ffg000fff\007'
printf '\033]10;#fffg00fff\007'
printf '\033]10;#fff0g0fff\007'
printf '\033]10;#fff00gfff\007'
printf '\033]10;#fff000gff\007'
printf '\033]10;#fff000fgf\007'
printf '\033]10;#fff000ffg\007'
# four nibble per color
printf '\033]10;#gFFF0000FFFF\007'
printf '\033]10;#FgFF0000FFFF\007'
printf '\033]10;#FFgF0000FFFF\007'
printf '\033]10;#FFFg0000FFFF\007'
printf '\033]10;#FFFFg000FFFF\007'
printf '\033]10;#FFFF0g00FFFF\007'
printf '\033]10;#FFFF00g0FFFF\007'
printf '\033]10;#FFFF000gFFFF\007'
printf '\033]10;#FFFF0000gFFF\007'
printf '\033]10;#FFFF0000FgFF\007'
printf '\033]10;#FFFF0000FFgF\007'
printf '\033]10;#FFFF0000FFFg\007'

# too short
printf '\033]10;#FF\007'
#in between
printf '\033]10;#F0FF\007'
printf '\033]10;#FF00F\007'
printf '\033]10;#FFF000F\007'
printf '\033]10;#FFF000FF\007'
printf '\033]10;#FFFF0000F\007'
printf '\033]10;#FFFF0000FF\007'
printf '\033]10;#FFFF0000FFF\007'
# too long
printf '\033]10;#FFFF0000FFFF0000\007'
