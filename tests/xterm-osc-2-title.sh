#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# set title
printf '\033]2;echo "fail"\n\007'

# query title, and device attributes
printf '\033]2;?\007\033[>c'

# set again title
printf '\033]2;title-v2\007'

# set empty
printf '\033]2;\007'

