#!/bin/sh

# fill space with E
printf '\033#8'

# set 50%, then the forms tmux sends
printf '\033]9;4;1;50\033\\'
printf '\033]9;4;3;50\033\\'
printf '\033]9;4;0;50\033\\'
# clamp
printf '\033]9;4;1;150\007'
# error keeps the value
printf '\033]9;4;2\007'
# malformed reports change nothing
printf '\033]9;4;1;5x\007'
printf '\033]9;4;7;10\007'
# paused with a value, trailing semicolon accepted
printf '\033]9;4;4;30;\033\\'
