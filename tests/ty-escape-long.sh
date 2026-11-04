#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# a 1 MiB terminology escape, ended by its NUL: nothing shows of it
printf '\033}x'
head -c 1048576 /dev/zero | tr '\0' 'A'
printf '\000'
printf 'A'
