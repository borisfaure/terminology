#!/bin/sh

# clear screen
printf '\033[2J'

##
# Every 4:x style draws an underline, like a plain 4
##

printf '\033[3;1H'
printf '\033[4mplain\033[m '
printf '\033[4:1msingle\033[m '
printf '\033[4:2mdouble\033[m '
printf '\033[4:3mcurly\033[m '
printf '\033[4:4mdotted\033[m '
printf '\033[4:5mdashed\033[m'

##
# 4:0 removes the underline and leaves the other attributes alone
##

printf '\033[5;1H'
printf '\033[1;4mboth\033[4:0m bold only\033[m'

##
# A subparameter must not be taken for the next parameter
##

printf '\033[7;1H'
printf '\033[4:3;1mbold underlined\033[m '
printf '\033[4;3mitalic underlined\033[m'

##
# Unknown, missing and extra subparameters still just underline
##

printf '\033[9;1H'
printf '\033[4:9munknown\033[m '
printf '\033[4:mempty\033[m '
printf '\033[4:3:7mextra\033[m'
