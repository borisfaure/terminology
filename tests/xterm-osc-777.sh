#!/bin/sh

# fill space with E
printf '\033#8'

# title and body
printf '\033]777;notify;Build;finished\033\\'
# the body keeps its semicolons
printf '\033]777;notify;Build;a;b;c\007'
# no body: dropped, as it always was
printf '\033]777;notify;lonely\007'
# not a notify command
printf '\033]777;preexec;ls\007'
# markup is the backend's business: recorded verbatim
printf '\033]777;notify;<b>t</b>;x & y\033\\'
