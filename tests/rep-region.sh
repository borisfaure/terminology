#!/bin/sh

# REP counts larger than the screen, within a scroll region and left and
# right margins 7 columns apart, one band of columns per case. Each case
# ends with a '.' at the cursor. The result must be that of the same
# characters written one by one, in rep-region-literal.sh.

# fill the screen with E, scroll region on rows 5 to 15
printf '\033#8\033[5;15r\033[?69h'

# inside the region
printf '\033[2;8s\033[7;4Ha\033[4999b.'

# above the region, from right of the right margin
printf '\033[10;16s\033[2;70Hb\033[2999b.'

# below the region: no scroll, the last row is rewritten
printf '\033[18;24s\033[18;20Hc\033[2999b.'

# double width
printf '\033[26;32s\033[9;27H中\033[3332b.'

# autowrap off
printf '\033[34;40s\033[?7l\033[11;35Hd\033[1999b.\033[?7h'

# insert mode
printf '\033[42;48s\033[4h\033[13;45He\033[2099b.\033[4l'
