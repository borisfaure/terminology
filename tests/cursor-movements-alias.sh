#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# goto 80;25 (CUP) and back to 0;0
printf '\033[26;80HZ'
printf '\033[H'

# RIGHT
# cursor right
printf '\033[aA\033[2aB\033[;aC'
# test on boundaries
printf '\033[2;80HZ\033[aA'

# LEFT
# go to 12;2 (CUP)
printf '\033[2;12H'
# cursor left
printf 'C\033[jD\033[;jc\033[3jd'
# Go left on start of line
printf '\033[2;0HE\033[jF'

# DOWN
# cursor down
printf '\033[eG\033[;eH\033[3eI'
# At Bottom
printf '\033[26;0HM\033[eN'

# UP
# cursor up
# go to 26;6 (CUP)
printf '\033[26;6H'
printf 'O\033[kP\033[;kQ\033[0kR'
# At top
printf '\033[0;26Hp\033[kq'

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
# LEFT
printf '\033[5;0H#\033[j<'
# UP
printf '\033[0;5H#\033[k^'
# DOWN
printf '\033[11;8H#\033[ev'
