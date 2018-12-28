#!/bin/sh
#set color
printf '\033[46;31;3m'

# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15\044x'
# restrict cursor
printf '\033[?6h'
# reset wrap mode
printf '\033[?7l'

# set tabs
printf '\033H\033H\033[3C\033H\033[4C\033H\033[5C\033H\033[6C\033H'
printf '\033[7C\033H\033[8C\033H\033[9C#'

# set cursor shape
printf '\033[4 q'

# fill space with E
printf '\033#8'

printf 'start'
#move to 0;0
printf '\033[HST'

printf '\n#\t#\033[2I\033[3g@\t#\t#\t#\t#\t#\t#\t#\t#\t#\t#'

# move to 1; 75
printf '\033[1;75Habcdefghijkl'
# shall be no wrap and in color. also no margin

#set margins again, cursor is still restricted
# set top/bottom margins:
printf '\033[10;20r'
# set left/right margins:
printf '\033[5;15s'
#move to 0;0
printf '\033[22;22H2'
