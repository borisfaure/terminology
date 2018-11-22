#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'
# restrict cursor
printf '\033[?6h'
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# fill larger rect with @
printf '\033[64;2;3;23;20$x'
# change color
printf '\033[0m\033[45;32;1m'
# top left corner with [
printf '\033[91;3;5;12;7$x'
# top right corner with ]
printf '\033[93;8;12;13;17$x'
# bottom left corner with (
printf '\033[40;18;3;22;8$x'
# top right corner with )
printf '\033[41;17;14;23;18$x'
# change color
printf '\033[0m\033[44;33;4m'
# one in center with #
printf '\033[35;12;12;12;12$x'

# cursor to 0,0
printf '\033[H'
echo '--------------------------------------------------'
