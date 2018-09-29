#!/bin/bash

# fill space with E
printf '\033#8'
# cursor to 0,0
printf '\033[H'
# restrict cursor
printf '\033[?6h'
# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'
# move to 0,0 with margins:
printf '\033[H'
# clean up rect
printf '\033[1;1;11;11$z'
echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'

I=0
while [ 1 ]; do
    sleep 2
    echo $I
    I=$((I + 1))
done
