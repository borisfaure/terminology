#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# set color
printf '\033[46;31;3m'

# APC cancelled by CAN
printf '\033_Gi=1,a=q;AAAA\030'
printf 'A'

# APC cancelled by SUB
printf '\033_Gi=1,a=q;AAAA\032'
printf 'B'

# APC introduced by the C1 APC, cancelled by CAN
printf '\302\237Gi=1,a=q;AAAA\030'
printf 'C'

# BEL does not end an APC: the string runs until CAN
printf '\033_title\007swallowed\030'
printf 'D'

# BEL does not end an APC: the string runs until ST
printf '\033_title\007swallowed\033\\'
printf 'E'

# CAN only cancels the APC, the next sequence is parsed
printf '\033_G\030\033[32mF'
