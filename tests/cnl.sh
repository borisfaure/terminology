#!/bin/sh
# fill space with E
printf '\033[69;1;1;25;80$x'
#set color
printf '\033[46;31;3m'

# move to 3; 3
printf '\033[3;3H'
printf 'a\033[Eb'

printf 'c\033[2Ed'
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15$x'

# move
printf '\033[19;19H'
printf 'e\033[5Ef'




# set left/right margins:
printf '\033[25;35s'
# fill margin with @
printf '\033[64;10;25;20;35$x'
# restrict cursor
printf '\033[?6h'
# move
printf '\033[10;10H'
printf 'g\033[5Eh'
