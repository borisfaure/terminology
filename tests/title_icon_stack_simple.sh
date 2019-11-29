#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# set title + icon
printf '\033]0;alpha\007'

# save both
printf '\033[22;0t'

# set again title + icon
printf '\033]0;bravo\007'

# save both
printf '\033[22;0t'

# set again title + icon
printf '\033]0;charlie\007'

# pop title (bravo)
printf '\033[23;2t'

# set again icon
printf '\033]1;delta\007'

# pop icon (alpha)
printf '\033[23;1t'
