#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'

# Set Bell volume to Off
printf '\033[1 t\007'
# Set Bell volume to Low
printf '\033[2 t\007'
printf '\033[3 t\007'
printf '\033[4 t\007'
# Set Bell volume to High
printf '\033[5 t\007'
printf '\033[6 t\007'
printf '\033[7 t\007'
printf '\033[8 t\007'
printf '\033[0 t\007'
printf '\033[43 t\007'
printf '\033[ t\007'
