#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# SD and RI in a scroll region that ends on the last row: the rows above
# the region must not move

printf '\033[2J\033[H'
i=1
while [ "$i" -le 24 ]; do
   printf '\033[%d;1Hrow %02d' "$i" "$i"
   i=$((i + 1))
done

# DECSTBM to the last row, then SD
printf '\033[3;24r\033[2T'

# RI at the top margin
printf '\033[3;1H\033M'

# a bottom margin given as the screen height or past it
printf '\033[6;99r\033[T\033[6;1H\033M'

printf '\033[r\033[H'
