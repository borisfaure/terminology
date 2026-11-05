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

# a 64 KiB sixel image: none of it shows
printf '\033Pq'
repeat 65536 '~'
printf '\033\\'
printf 'A'

# an ESC not followed by '\' does not end the DCS
printf '\033Pq'
repeat 8192 '~'
printf '\033X'
repeat 8192 '~'
printf '\033\\'
printf 'B'

# a long DCS ended by the C1 ST
printf '\033Pq'
repeat 8192 '~'
printf '\302\234'
printf 'C'

# a long DCS cancelled by CAN
printf '\033Pq'
repeat 8192 '~'
printf '\030'
printf 'D'

# a long DCS cancelled by SUB, the next sequence is parsed
printf '\033Pq'
repeat 8192 '~'
printf '\032\033[32mE'
