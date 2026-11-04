#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# $1: the number of times to repeat $2
repeat()
{
    head -c "$1" /dev/zero | tr '\0' "$2"
}

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# a 1 MiB APC: none of it shows
printf '\033_Ga=T,f=100;'
repeat 1048576 A
printf '\033\\'
printf 'A'

# an ESC not followed by '\' does not end the APC
printf '\033_G'
repeat 65536 B
printf '\033X'
repeat 65536 B
printf '\033\\'
printf 'B'

# a long APC introduced by the C1 APC, cancelled by CAN
printf '\302\237G'
repeat 65536 C
printf '\030'
printf 'C'

# a long APC cancelled by SUB, the next sequence is parsed
printf '\033_G'
repeat 65536 D
printf '\032\033[32mD'
