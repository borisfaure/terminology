#!/bin/sh

# fill space with E
printf '\033#8'

printf '\033]0;kept\007'
# a number and nothing after it: must not read past the payload
printf '\033]0\007'
printf '\033]2\033\\'
printf '\033]1\007'
printf '\033]4\007'
printf '\033]4\033\\'
printf '\033]7;file://localhost/tmp\007'
printf '\033]7\007'
printf '\033]8\007'
printf '\033]10\007'
printf '\033]11\033\\'
printf '\033]12\007'
printf '\033]52\007'
printf '\033]777\007'
printf '\033]99\007'
printf '\033]9\033\\'
