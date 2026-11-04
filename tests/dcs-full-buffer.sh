#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# $1: the number of spaces to print
spaces()
{
    head -c "$1" /dev/zero | tr '\0' ' '
}

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# a DECRQSS whose ESC \ starts on the last codepoint of the DCS buffer: it
# is still a request, answered as invalid
printf '\033P$q'
spaces 4092
printf 'm\033\\'
printf 'A'

# a codepoint more and the DCS overflows: it is discarded up to its ST
printf '\033P$q'
spaces 4094
printf 'm\033\\'
printf 'B'

# a long sixel image, ignored
printf '\033Pq'
i=0
while [ $i -lt 100 ]; do
    printf '#0;2;0;0;0#0~~@@vv@@~~$-'
    i=$((i + 1))
done
printf '\033\\'
printf 'C'
