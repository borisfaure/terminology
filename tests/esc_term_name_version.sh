#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80\044x'

#set color
printf '\033[46;31;3m'

# move
printf '\033[H'

# print terminal + version
printf '\033[>0q'

# let it print
sleep 0.2

# move
printf '\033[2H'

# print terminal + version
printf '\033[>q'

# let it print
sleep 0.2
