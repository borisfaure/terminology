#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

#pop none
printf '\033[23;0t'

# set title + icon
printf '\033]0;alpha\007'

# save both (default case)
printf '\033[22t'

# restack last elem
printf '\033[22;3t'

# set again title + icon
printf '\033]0;bravo\007'

# pop both (default case)
printf '\033[23t'

# set again title + icon
printf '\033]0;charlie\007'


# pop both (default case)
printf '\033[23t'

# pop none (default case)
printf '\033[23t'
