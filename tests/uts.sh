#!/bin/sh

# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# move to 0;0
printf '\033[H'
# set tabs
printf '\033H\033H\033[3C\033[W\033[4C\033[0W\033[5C\033H\033[6C\033H'
printf '\033[7C\033H\033[8C\033H\033[9C'
printf '\nTabs set:'
# show # on tabs
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
printf '\nRemoved tab on position 3'
# remove tab on col 0 (has no effect)
printf '\033[ d'
# remove tab on col 3
printf '\033[3 d'
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
