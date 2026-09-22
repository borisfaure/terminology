#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# set color
printf '\033[46;31;3m'

# kitty graphics support query, as sent by some CLIs on startup
printf '\033_Gi=31,s=1,v=1,a=q,t=d,f=24;AAAA\033\\'
printf 'A'

# APC terminated by the C1 ST
printf '\033_Gi=1,a=q;AAAA\302\234'
printf 'B'

# APC whose payload holds an ESC that does not start a ST
printf '\033_G\033[31mfoo\033\\'
printf 'C'

# empty APC
printf '\033_\033\\'
printf 'D'

# APC introduced by the C1 APC
printf '\302\237Gi=1,a=q;AAAA\033\\'
printf 'E'
