#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 2,0
printf '\033[2H'

# set text
printf "Insanity is doing the same thing, over and over again, but expecting different results"
# force render
printf '\033}tr\0'

## Select simple word: "thing"
# double-click to select word
printf '\033}td;200;20;1;0;0\0'
printf '\033}tu;200;20;1;0;0\0'
printf '\033}td;200;20;1;0;1\0'
printf '\033}tu;200;20;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsthing\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## Select last word: "results"
# double-click to select word
printf '\033}td;20;35;1;0;0\0'
printf '\033}tu;20;35;1;0;0\0'
printf '\033}td;20;35;1;0;1\0'
printf '\033}tu;20;35;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsresults\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## Same but by the start of the word, at the end of the previous line
# double-click to select word
printf '\033}td;555;20;1;0;0\0'
printf '\033}tu;555;20;1;0;0\0'
printf '\033}td;555;20;1;0;1\0'
printf '\033}tu;555;20;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsresults\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## Select first word: "Insanity"
# There was a bug where only the part of the word on the left of the mouse
# cursor was selected
# double-click to select word
printf '\033}td;40;20;1;0;0\0'
printf '\033}tu;40;20;1;0;0\0'
printf '\033}td;40;20;1;0;1\0'
printf '\033}tu;40;20;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsInsanity\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# Now with a line before
# move to 1,0
printf '\033[1H'
# insert line
printf 'Quote:'
# double-click to select word
printf '\033}td;10;20;1;0;0\0'
printf '\033}tu;10;20;1;0;0\0'
printf '\033}td;10;20;1;0;1\0'
printf '\033}tu;10;20;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsInsanity\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'
