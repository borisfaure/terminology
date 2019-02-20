#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80\044x'

#set color
printf '\033[46;31;3m'

# move
printf '\033[H'

#
# Primary attributes
#

# default value
printf '\033[c'
# 0
printf '\033[0c'
# invalid value
printf '\033[42c'

#
# Secondary attributes
#

# default value
printf '\033[>c'
# 0
printf '\033[>0c'
# invalid value
printf '\033[>42c'

#
# Tertiary attributes
#

# default value
printf '\033[=c'
# 0
printf '\033[=0c'
# invalid value
printf '\033[=42c'

# let it print
sleep 0.2
