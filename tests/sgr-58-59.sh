#!/bin/sh

# clear screen
printf '\033[2J'

##
# semicolon forms do not leak into other attributes
##

printf '\033[3;1H'
printf '\033[58;5;1mA\033[m '
printf '\033[58;2;59;34;76mB\033[m '
printf '\033[58;5;256mC\033[m '
printf '\033[58;5mD\033[m'

##
# colon forms: index, RGB with and without colour space, extras
##

printf '\033[5;1H'
printf '\033[58:5:1mE\033[m '
printf '\033[58:2::59:34:76mF\033[m '
printf '\033[58:2:0:59:34:76mG\033[m '
printf '\033[58:2:59:34:76mH\033[m '
printf '\033[58:2::59:34:76:0:1:2mI\033[m'

##
# what follows 58 still applies
##

printf '\033[7;1H'
printf '\033[58;5;1;1mbold\033[m '
printf '\033[58;2;59;34;76;9mstrike\033[m '
printf '\033[58:2::59:34:76;3mitalic\033[m '
printf '\033[1;58:5:9;4mbold underlined\033[m'

##
# 59, alone and among others
##

printf '\033[9;1H'
printf '\033[59mJ\033[m '
printf '\033[1;59;3mbold italic\033[m '
printf '\033[59:1;1mbold\033[m'

##
# missing and unknown colour types
##

printf '\033[11;1H'
printf '\033[58mK\033[m '
printf '\033[58;mL\033[m '
printf '\033[58;7;1mbold\033[m '
printf '\033[58:7:1;3mitalic\033[m'

##
# the transparent type: extra subparameters are dropped in the colon
# form, kept as separate parameters in the semicolon form
##

printf '\033[13;1H'
printf '\033[58:1:5mplain\033[m '
printf '\033[38:1:5minvisible\033[m '
printf '\033[38;1;5minvisible blink\033[m'
