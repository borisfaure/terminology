#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# DECANM: ANSI mode is the only mode, nothing locks
printf '\033[?2h\033[2\044p\033[?2\044p'
printf '\033[?2l\033[2\044p\033[?2\044p'
# KAM, alone and among other modes
printf '\033[2h\033[2\044p\033[2l\033[2\044p'
printf '\033[4;2h\033[2\044p\033[4\044p\033[2;4l\033[2\044p\033[4\044p'
# DECSTR and RIS unlock
printf '\033[2h\033[!p\033[2\044p'
printf '\033[2h\033c\033[2\044p'
