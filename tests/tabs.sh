#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'
printf '\033H\033H\033[3C\033H\033[4C\033H\033[5C\033H\033[6C\033H'
printf '\033[7C\033H\033[8C\033H\033[9C#'
printf '\n#\t#\033[2I#\t#\033[3Z\033[g@\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
printf '\n#\t#\033[2I\033[3g@\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'
