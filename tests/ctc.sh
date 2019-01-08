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
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
# remove and add a tab
printf '\n#  \033[2W@      \033[0W#   \033[W#(One tab removed, two added)'
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
# remove all tabs
printf '\n\033[4WRemove all tabs:'
printf '\n#\t#\t#\t#'
# set tabs
printf '\nAdd them again\033[?5W:'
printf '\r\033H\033H\033[3C\033[W\033[4C\033[0W\033[5C\033H\033[6C\033H'
printf '\033[7C\033H\033[8C\033H\033[9C'
printf '\n#\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
# remove all tabs
printf '\n\033[5WRemove all tabs:'
printf '\n#\t#\t#\t#'
