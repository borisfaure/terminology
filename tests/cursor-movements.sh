#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# goto 80;25 (CUP) and back to 0;0
printf '\033[26;80HZ'
printf '\033[H'

# RIGHT
# cursor right HPR / CUU
printf 'A\033[aB\033[;aa\033[0ab'
printf '\033[CA\033[2CB\033[;CC'
# test on boundaries
printf '\033[1;80HZ\033[aA'
printf '\033[2;80HZ\033[CA'

# LEFT
# go to 12;2 (CUP)
printf '\033[2;12H'
# cursor left (CUB)
printf 'C\033[DD\033[;Dc\033[3Dd'
# Go left on start of line
printf '\033[2;0HE\033[DF'

# DOWN
# cursor down
printf '\033[BG\033[;BH\033[3BI'
printf '\033[eJ\033[;eK\033[3eL'
# At Bottom
printf '\033[26;0HM\033[BN'
printf '\033[26;3Hm\033[en'

# UP
# cursor up
# go to 26;6 (CUP)
printf '\033[26;6H'
printf 'O\033[AP\033[;AQ\033[0AR'
# At top
printf '\033[0;26Hp\033[Aq'

# WITH CURSOR RESTRICTION
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[15;30s'
# change color
printf '\033[0m\033[45;32;1m'
# fill restricted region with @
printf '\033[64;10;15;20;30\044x'
# change color back
printf '\033[0m\033[46;31;3m'
# restrict cursor
printf '\033[?6h'
# RIGHT
printf '\033[5;16H#\033[a>'
printf '\033[6;16H#\033[C>'
# LEFT
printf '\033[5;0H#\033[D<'
# UP
printf '\033[0;5H#\033[A^'
# DOWN
printf '\033[11;6H#\033[ev'
printf '\033[11;8H#\033[Bv'
