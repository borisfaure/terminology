#!/bin/sh

# move to 0; 0
printf '\033[H'
# fill space
for _ in $(seq 0 23); do
   for _ in $(seq 0 7); do
      printf '\-/|\\~_>^<'
   done
done

# move to 0; 0
printf '\033[H'

#set color
printf '\033[46;31;3m'
# set top/bottom margins:
printf '\033[2;23r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[3;78s'

# restrict cursor
printf '\033[?6h'

# DECFRA
# fill inside rect with nothing
printf '\033[;10;40;15;45\044x'
# fill inside rect with invalid value
printf '\033[30;10;40;15;45\044x'
# top left corner
# @xtermbug
printf '\033[64;;;10;10\044x'
# bottom right corner
printf '\033[64;17;70;;\044x'

# DECERA
# top right corner
printf '\033[;70;10;\044z'
# bottom left corner
# @xtermbug
printf '\033[17;;;10\044z'
