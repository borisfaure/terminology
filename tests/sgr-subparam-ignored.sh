#!/bin/sh

# clear screen
printf '\033[2J'

##
# a code that takes no subparameter is ignored with them
##

printf '\033[3;1H'
printf '\033[1:2mneither\033[m '
printf '\033[3;9:1;1mitalic bold\033[m '
printf '\033[24:1;4munderlined\033[m '
printf '\033[73:1:2;1mbold\033[m'

##
# 58 is not known yet: its subparameters do not leak
##

printf '\033[5;1H'
printf '\033[58:5:1mplain\033[m '
printf '\033[58:2::59:34:76mplain\033[m '
printf '\033[58:2:59:34:76;3mitalic\033[m'

##
# extra subparameters after an index or a bad colour type are dropped
##

printf '\033[7;1H'
printf '\033[38:5:1:7mred not inverse\033[m '
printf '\033[48:5:2:7:1;3mgreen bg italic\033[m '
printf '\033[38:7:1;3mitalic\033[m'
