#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# cursor to 7,4
printf '\033[7;4H'
# Request cursor position
printf '\033[6n'
# Request cursor position (dec)
printf '\033[?6n'

test_sleep 0.2

# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;15s'

# cursor to 17,14
printf '\033[17;14H'

# Request cursor position
printf '\033[6n'
# Request cursor position (dec)
printf '\033[?6n'

test_sleep 0.2

# restrict cursor
printf '\033[?6h'

# Request cursor position
printf '\033[6n'
# Request cursor position (dec)
printf '\033[?6n'
