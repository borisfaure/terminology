#!/bin/sh

# clear screen
printf '\033[2J\033[H'

# two columns cannot hold left and right margins
printf '\033[8;5;2t'
printf '\033[?69h'
printf '\033[s'
printf 'abcdefgh'
printf '\033[T\033[S\033[L\033[M'
printf '\033[1;2s'
printf '\033[2;1H\033[@x\033[P'

printf '\033[8;24;80t'
printf '\033[10;1Hdone'
