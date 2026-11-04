#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# $1: the number of times to repeat $2
repeat()
{
    head -c "$1" /dev/zero | tr '\0' "$2"
}

# An OSC split across reads resumes its scan where it stopped, and is set
# once whatever the read sizes

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# a long title closed by BEL
printf '\033]2;'
repeat 600 T
printf '\007'
printf 'A'

# a long title closed by ESC backslash, whose ESC may fall on a read
# boundary
printf '\033]2;'
repeat 600 U
printf '\033\\'
printf 'B'

# an OSC longer than the 4096 codepoint buffer is discarded up to its ST
printf '\033]2;'
repeat 4100 V
printf '\007X'
printf 'C'
