#!/bin/sh

# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# set tabs
printf '\033H\033H\033[3C\033H\033[4C\033H\033[5C\033H\033[6C\033H'
printf '\033[7C\033H\033[8C\033H\033[9C'
# show # on tabs
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
printf '\033[?5W'
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
