#!/bin/sh

#set color
printf '\033[46;31;3m'
# fill space with E
printf '\033[69;1;1;25;80$x'
# restrict cursor
printf '\033[?6h'
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# top left corner
printf '\033[3;5;12;7$z'
# top right corner
printf '\033[8;12;13;17$z'
# bottom left corner
printf '\033[18;3;22;8$z'
# top right corner
printf '\033[17;14;23;18$z'
