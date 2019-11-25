#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# set title + icon
printf '\033]0;foobar\007'

# set again title + icon with no command
printf '\033];no command\007'

# set again title + icon with id as double
printf '\033]00;double\007'

# set again title + icon with id as hex
printf '\033]0x0;hex\007'

# set again title + icon with id as negative zero
printf '\033]-0;negative zero\007'

# set again title + icon with id as negative value
printf '\033]-2;negative value\007'

# set again title + icon with space
printf '\033] 0;with spaces\007'

# set again title + icon with space
printf '\033]0 ;with spaces v2\007'

# set again title + icon with overflow
printf '\033]99999999999999999999999999999999999999999999999999;overflow\007'
