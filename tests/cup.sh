#!/bin/sh
# fill space with E
printf '\033[69;1;1;25;80$x'
#set color
printf '\033[46;31;3m'

# move to 7; 7
printf '\033[7;7HA'
printf '\033[;8HB'
printf '\033[8HC'
printf '\033[9;HD'
printf '\033[0;0HE'

# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15$x'

# move
printf '\033[12;19HF'

# set left/right margins:
printf '\033[25;35s'
# fill margin with @
printf '\033[64;10;25;20;35$x'
# restrict cursor
printf '\033[?6h'
# move
printf '\033[2;2HG'
