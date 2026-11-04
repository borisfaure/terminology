#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# a 1 MiB line, wrapped over and over into a single backlog row
head -c 1048576 /dev/zero | tr '\0' 'A'
printf '\r\nB'
