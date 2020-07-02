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
# one nibble per color
printf '\033]10;rgb:f/0/f\007'
# two nibble per color
printf '\033]10;rgb:ff/00/ff\007'
# three nibble per color
printf '\033]10;rgb:fff/000/fff\007'
# four nibble per color
printf '\033]10;rgb:FFFF/0000/FFFF\007'

##
# invalid
##

# not hex
# one nibble per color
printf '\033]10;rgb:g/0/f\007'
printf '\033]10;rgb:f/g/f\007'
printf '\033]10;rgb:f/0/g\007'
# two nibble per color
printf '\033]10;rgb:gf/00/ff\007'
printf '\033]10;rgb:fg/00/ff\007'
printf '\033]10;rgb:ff/g0/ff\007'
printf '\033]10;rgb:ff/0g/ff\007'
printf '\033]10;rgb:ff/00/gf\007'
printf '\033]10;rgb:ff/00/fg\007'
# three nibble per color
printf '\033]10;rgb:gff/000/fff\007'
printf '\033]10;rgb:fgf/000/fff\007'
printf '\033]10;rgb:ffg/000/fff\007'
printf '\033]10;rgb:fff/g00/fff\007'
printf '\033]10;rgb:fff/0g0/fff\007'
printf '\033]10;rgb:fff/00g/fff\007'
printf '\033]10;rgb:fff/000/gff\007'
printf '\033]10;rgb:fff/000/fgf\007'
printf '\033]10;rgb:fff/000/ffg\007'
# four nibble per color
printf '\033]10;rgb:gFFF/0000/FFFF\007'
printf '\033]10;rgb:FgFF/0000/FFFF\007'
printf '\033]10;rgb:FFgF/0000/FFFF\007'
printf '\033]10;rgb:FFFg/0000/FFFF\007'
printf '\033]10;rgb:FFFF/g000/FFFF\007'
printf '\033]10;rgb:FFFF/0g00/FFFF\007'
printf '\033]10;rgb:FFFF/00g0/FFFF\007'
printf '\033]10;rgb:FFFF/000g/FFFF\007'
printf '\033]10;rgb:FFFF/0000/gFFF\007'
printf '\033]10;rgb:FFFF/0000/FgFF\007'
printf '\033]10;rgb:FFFF/0000/FFgF\007'
printf '\033]10;rgb:FFFF/0000/FFFg\007'

# too short
printf '\033]10;rgb:/F/F\007'
printf '\033]10;rgb:F//F\007'
printf '\033]10;rgb:F/F/\007'
# too long
printf '\033]10;rgb:FFFFF/0000/FFFF\007'
printf '\033]10;rgb:FFFF/00000/FFFF\007'
printf '\033]10;rgb:FFFF/0000/FFFFF\007'
printf '\033]10;rgb:FFFF/0000/FFFF/\007'
printf '\033]10;rgb:FFFF/0000/FFFF/0000\007'
