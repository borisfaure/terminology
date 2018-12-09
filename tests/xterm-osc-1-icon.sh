#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# set icon
printf '\033]1;echo "fail"\n\007'

# query icon, and device attributes
printf '\033]1;?\007\033[>c'

# set again icon
printf '\033]1;icon-v2\007'

# set empty
printf '\033]1;\007'

