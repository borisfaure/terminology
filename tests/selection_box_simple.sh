#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'

##
# Mouse box selection with ctrl
##

# mouse down to start box selection (with ctrl=4)
printf '\033}td;20;10;1;4;0\0'

# mouse move
printf '\033}tm;48;28;4\0'

# mouse up (with ctrl=4)
printf '\033}tu;48;28;1;4;0\0'

# force render
printf '\033}tr\0'

# selection is 'EEEEE\nEEEEE'
printf '\033}tsEEEEE\nEEEEE\0'

# insert E in color
printf '\033[;4HE'

# force render
printf '\033}tr\0'

# selection is 'EEEEE\nEEEEE'
printf '\033}tsEEEEE\nEEEEE\0'

# insert a
printf 'a'

# force render
printf '\033}tr\0'

# no more selection
printf '\033}tn\0'

##
# Same with ctrl unset on mouse up
##

# fill space with E
printf '\033#8'

# mouse down to start box selection (with ctrl=4)
printf '\033}td;20;10;1;4;0\0'

# mouse move (no ctrl)
printf '\033}tm;48;28;0\0'

# mouse up (ctrl is no needed here)
printf '\033}tu;48;28;1;0;0\0'

# force render
printf '\033}tr\0'

# selection is 'EEEEE\nEEEEE'
printf '\033}tsEEEEE\nEEEEE\0'

# insert E in color
printf '\033[;4HE'

# force render
printf '\033}tr\0'

# selection is 'EEEEE\nEEEEE'
printf '\033}tsEEEEE\nEEEEE\0'

# insert a
printf 'a'

# force render
printf '\033}tr\0'

# no more selection
printf '\033}tn\0'
