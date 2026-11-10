#!/bin/sh

# clear screen
printf '\033[2J\033[H'

# on a single column, a tab cannot move the cursor
printf '\033[8;5;1t'

printf '\tA\r\n'
printf 'B\tC\r\n'
printf '\t\t\r\n'

printf '\033[8;24;80t'
printf '\033[10;1Hdone\t.'
