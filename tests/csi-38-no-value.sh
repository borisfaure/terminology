#!/bin/sh

# clear screen
printf '\033[2J'

# fill space with E
printf '\033#8'

##
# Test foreground
##

# move
printf '\033[4;4H'
# set color
printf '\033[m\033[46;31;3m'
printf 'foo'
#set RGB
printf '\033[48:2:244:144:25;38:2:56:150:199m'
printf 'bar'
printf '\033[1;38m'
printf 'qux'


##
# Test background
##

# move
printf '\033[8;4H'
# set color
printf '\033[m\033[46;31;3m'
printf 'foo'
#set RGB
printf '\033[48:2:244:144:25;38:2:56:150:199m'
printf 'bar'
printf '\033[1;48m'
printf 'qux'

