#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# the numLock mode (?1035): set (the default), Num_Lock forces the
# numeric keypad even under DECPAM; reset, DECPAM wins
printf '\033[?1035\044p'
printf '\033[?1035l\033[?1035\044p'
printf '\033[?1035h\033[?1035\044p'

# DECSTR and RIS restore the default
printf '\033[?1035l\033[!p\033[?1035\044p'
printf '\033[?1035l\033c\033[?1035\044p'
