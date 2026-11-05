#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

# CAN cancels a DCS, the text after it shows
printf '\033Pqabc\030A'

# so does SUB
printf '\033P$qm\032B'

# a cancelled DECRQSS is not answered, the next sequence is parsed
printf '\033P$qm\030\033[32mC'

# CAN right after the introducer
printf '\033P\030D'
