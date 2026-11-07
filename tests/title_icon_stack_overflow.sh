#!/bin/sh

# push 12 titles; the stack keeps the 10 most recent, like xterm and kitty
for i in 0 1 2 3 4 5 6 7 8 9 10 11; do
   printf '\033]0;title%s\007' "$i"
   printf '\033[22;0t'
done
printf '\033]0;top\007'

# pop all of them and more: the last pop to succeed restores title2
for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13; do
   printf '\033[23;0t'
done
