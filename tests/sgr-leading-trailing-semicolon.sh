#!/bin/sh

# fill space with E
printf '\033#8'
# cursor to 0,0
printf '\033[H'

printf '\e[31;mabcd\n'
printf '\e[;31mabcd\n'
printf '\e[46;;31mabcd\n'
