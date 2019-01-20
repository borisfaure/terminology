#!/bin/sh

# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# move to 2;2
printf '\033[2;2H'

# VPA 0
printf '\033[0d0'

# VPA 1
printf '\033[2;4H'
printf '\033[1d1'

# VPA 2
printf '\033[2;6H'
printf '\033[2d2'

# VPA default
printf '\033[2;8H'
printf '\033[ddefault'

# VPA 23
printf '\033[2;6H'
printf '\033[23d23'

# VPA 24
printf '\033[2;6H'
printf '\033[24dbottom'

# VPA too far
printf '\033[2;20H'
printf '\033[33333dtoo far'


# VPA with margins
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[25;50s'
# change color
printf '\033[0m\033[45;32;1m'
# fill restricted region with @
printf '\033[64;10;25;20;50\044x'
# change color back
printf '\033[0m\033[46;31;3m'
printf '\033[2;30H'
printf '\033[22d22 with margins'

# VPA with cursor restricted
# restrict cursor
printf '\033[?6h'
printf '\033[2;20H'
printf '\033[22d22 with margins and DECOM'
