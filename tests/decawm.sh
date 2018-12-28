#!/bin/sh
#set color
printf '\033[46;31;3m'
# fill space with E
printf '\033#8'

# set wrap mode
printf '\033[?7h'
# move to 1; 75
printf '\033[1;75Habcdefghijkl'
# reset wrap mode
printf '\033[?7l'
# move to 3; 75
printf '\033[3;75Habcdefghijkl'


# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15$x'

# set wrap mode
printf '\033[?7h'
# move
printf '\033[12;12Habcdefghijkl'
# reset wrap mode
printf '\033[?7l'
# move
printf '\033[15;12Habcdefghijkl'


# set left/right margins:
printf '\033[25;35s'
# fill margin with @
printf '\033[64;10;25;20;35$x'
# restrict cursor
printf '\033[?6h'
# set wrap mode
printf '\033[?7h'
# move
printf '\033[2;5Habcdefghijkl'
# reset wrap mode
printf '\033[?7l'
# move
printf '\033[5;5Habcdefghijkl'
