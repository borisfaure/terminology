#!/bin/sh
# fill space with E
printf '\033[69;1;1;25;80\044x'
#set color
printf '\033[46;31;3m'

# move to 7; 7
printf '\033[7;7fA'
printf '\033[;8fB'
printf '\033[8fC'
printf '\033[9;fD'
printf '\033[0;0fE'

# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15\044x'

# move
printf '\033[12;19fF'

# Test outside margins
printf '\033[22;22fG'

# set left/right margins:
printf '\033[25;35s'
# fill margin with @
printf '\033[64;10;25;20;35\044x'
# restrict cursor
printf '\033[?6h'
# move
printf '\033[2;2fH'

# Test outside margins
printf '\033[22;22fI'

