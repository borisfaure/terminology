#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# SU, SD, IL and DL with counts from one line to more than the region holds,
# in scroll regions with and without left/right margins

# fill a band of rows with lines that tell each row apart
fill() {
   printf '\033[r'
   i=$1
   while [ "$i" -le "$2" ]; do
      printf '\033[%d;1Hrow %02d ' "$i" "$i"
      printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-*/='
      i=$((i + 1))
   done
}

# a taller screen, for every case to keep its own band of rows
printf '\033[8;40;80t\033[2J\033[H'

# SU and SD of several lines in a region
fill 1 8
printf '\033[1;8r\033[3S\033[2T'

# DL and IL from the middle of a region, with a background colour for the
# opened lines
fill 9 14
printf '\033[9;14r\033[11;5H\033[44m\033[2M\033[13;1H\033[3L\033[0m'

# an IL count larger than what is left below the cursor, then a DL
fill 15 20
printf '\033[15;20r\033[18;1H\033[9L\033[15;1H\033[2M'

# left/right margins too
fill 21 24
printf '\033[21;24r\033[?69h\033[20;60s'
printf '\033[1S\033[2T\033[22;30H\033[1L\033[21;30H\033[1M'
printf '\033[?69l'

# hyperlinked rows scrolled by several lines at once, some of them twice
fill 25 30
printf '\033[27;1H\033]8;;http://example.org/a\033\\link a\033]8;;\033\\'
printf '\033[29;1H\033]8;;http://example.org/b\033\\link b\033]8;;\033\\'
printf '\033[25;30r\033[3S\033[2T\033[26;1H\033[2M\033[1L'

# a count of zero or none is one line
fill 31 36
printf '\033[31;36r\033[0S\033[T\033[33;1H\033[0L\033[M'

# SU and SD of more lines than the region holds
fill 37 40
printf '\033[37;38r\033[30S\033[39;40r\033[9T'

# a region of the whole screen
printf '\033[1;40r\033[2S\033[1T'

printf '\033[r\033[H'
