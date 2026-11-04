#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# C0 controls within a CSI are executed once, wherever the reads split it

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# three LFs within a SGR
printf '\033[1\n\n\n32mA'

# a BS and a HT within a SGR
printf '\033[\b\t0mB'

# LFs within a CSI longer than a read, ended by a SGR
printf '\033[3'
i=0
while [ $i -lt 64 ]; do
    printf ';3'
    i=$((i + 1))
done
printf "\n\n1mC"
