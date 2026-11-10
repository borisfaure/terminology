#!/bin/sh

# clear screen
printf '\033[2J\033[H'

# a single row cannot hold top and bottom margins
printf '\033[8;1;20t'
printf '\033[1r'
printf 'abc\033[F\033[J\033[K'
printf '\033[1;1r'
printf 'def\033[E\033[1J'

# on 24 rows, a scroll region cannot start on the last one
printf '\033[8;24;80t'
printf '\033[24;25r'
printf '\033[10;1Hdone\r\n'
