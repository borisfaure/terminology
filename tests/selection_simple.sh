#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'

# mouse down to start selection
printf '\033}td;20;10;1;0;0\0'

# mouse move
printf '\033}tm;48;10\0'

# mouse up
printf '\033}tu;48;10;1;0;0\0'

# selection is 'EEEEE'
printf '\033}tiEEEEE\0'

# insert E in color
printf '\033[;4HE'
# selection is 'EEEEE'
printf '\033}tiEEEEE\0'

# insert a
printf 'a'
# no more selection
printf '\033}tn'
