#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80\044x'

#set color
printf '\033[46;31;3m'

# arg = 3
printf '\033[1;10H1\033[3`1'

# no arg
printf '\033[2;10H2\033[`2'

# go too far
printf '\033[3;10H3\033[3333`3'

# arg == 0
printf '\033[4;10H4\033[0`4'

# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill margin with @
printf '\033[64;10;5;20;15\044x'

# From inside to outside
printf '\033[12;12HA\033[20`A'

# From outside (on the left) to outside (on the right)
printf '\033[12;2HB\033[40`B'

# restrict cursor
printf '\033[?6h'

# From inside to inside
printf '\033[4;4HC\033[6`C'

# From inside to outside
printf '\033[5;2HD\033[40`D'
