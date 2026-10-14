#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# IRM, reset then set
printf '\033[4\044p'
printf '\033[4h\033[4\044p\033[4l\033[4\044p'
# not handled by SM/RM: 0
printf '\033[1\044p\033[2\044p\033[3\044p\033[12\044p\033[20\044p'
printf '\033[9999\044p'
# no reply: missing mode, other private markers
printf '\033[\044p'
printf '\033[=4\044p'
# the private form still answers
printf '\033[?4\044p'
