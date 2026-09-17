#!/bin/sh

# clear screen
printf '\033[2J'

##
# 53 sets the overline, 55 removes it
##

printf '\033[3;1H'
printf '\033[53moverlined\033[55m plain'

##
# the overline and the underline are independent
##

printf '\033[5;1H'
printf '\033[4;53mboth\033[24m overlined\033[53;4m both\033[55m underlined\033[m'

##
# 54 resets framed and encircled, and leaves the overline alone
##

printf '\033[7;1H'
printf '\033[51;52;53mframed encircled overlined\033[54m overlined\033[m'

##
# a reset clears it
##

printf '\033[9;1H'
printf '\033[53moverlined\033[0m plain'
