#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# set title + icon
printf '\033]0;echo "fail"\n\007'

# query title + icon, and device attributes
printf '\033]0;?\007\033[>c'

# set again title + icon
printf '\033]0;title-icon-v2\007'

# set empty
printf '\033]0;\007'

