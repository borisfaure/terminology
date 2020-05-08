#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80\044x'

#set color
printf '\033[46;31;3m'

# move
printf '\033[H'

# 3 loops of 128 colors
for i in $(seq 255); do
   printf "\033[48;2;0;%s;%sm " "$i" "$i"
done
for i in $(seq 255); do
   printf "\033[48;2;%s;0;%sm " "$i" "$i"
done
for i in $(seq 255); do
   printf "\033[48;2;%s;%s;0m " "$i" "$i"
done
