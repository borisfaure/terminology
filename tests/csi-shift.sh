#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# SO and SI within a CSI shift the character sets, and ENQ
# sends the answerback, empty

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# DEC special graphics as G1
printf '\033)0'

# SO within a SGR 1: a bold line drawing q
printf '\033[1\016mq'

# SI within a SGR 0: back to an ASCII q
printf '\033[\0170mq'

# ENQ within a SGR 32: nothing is sent
printf '\033[3\0052mE'

# SO within a CSI longer than a read
printf '\033[3'
i=0
while [ $i -lt 64 ]; do
    printf ';3'
    i=$((i + 1))
done
printf '\016mq\017'
