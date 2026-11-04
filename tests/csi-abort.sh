#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# an ESC within a CSI aborts it and starts a new sequence, and
# CAN or SUB cancel it

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# an ESC aborts the SGR 31, the SGR 32 after it applies
printf '\033[31\033[32mA'

# an ESC right after the introducer
printf '\033[\033[0;7mB'

# the same with the C1 CSI
printf '\302\233\033[0;33mC\302\2331\033[0mD'

# CAN cancels the SGR 1: its final byte shows as text
printf '\033[1\030mE'

# SUB cancels the SGR 4
printf '\033[4\032mF'

# a LF before the CAN is still executed
printf '\033[1\n\030G'

# a CSI longer than a read, cancelled by CAN
printf '\033[3'
i=0
while [ $i -lt 64 ]; do
    printf ';3'
    i=$((i + 1))
done
printf '\030H'
