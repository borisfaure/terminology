#!/bin/sh

# rep-region.sh with each REP written out as the characters it repeats:
# both must end in the same state.

# fill the screen with E, scroll region on rows 5 to 15
printf '\033#8\033[5;15r\033[?69h'

# inside the region
printf '\033[2;8s\033[7;4H' ; printf 'a%.0s' $(seq 5000); printf '.'

# above the region, from right of the right margin
printf '\033[10;16s\033[2;70H' ; printf 'b%.0s' $(seq 3000); printf '.'

# below the region: no scroll, the last row is rewritten
printf '\033[18;24s\033[18;20H' ; printf 'c%.0s' $(seq 3000); printf '.'

# double width
printf '\033[26;32s\033[9;27H' ; printf '中%.0s' $(seq 3333); printf '.'

# autowrap off
printf '\033[34;40s\033[?7l\033[11;35H' ; printf 'd%.0s' $(seq 2000); printf '.\033[?7h'

# insert mode
printf '\033[42;48s\033[4h\033[13;45H' ; printf 'e%.0s' $(seq 2100); printf '.\033[4l'
