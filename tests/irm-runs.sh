#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# runs of plain ASCII typed in insert mode (IRM), which shift the rest of
# the row as one character at a time would

# clear screen, fill each row with a pattern to push aside
printf '\033[2J\033[H'
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do
   printf '\033[%d;1H' "$i"
   printf 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-*/ABCDEFGHIJKL'
done

printf '\033[4h'

# a word in the middle of a row
printf '\033[1;10Hword'

# a run that exactly fills the room left before the right edge
printf '\033[2;71H0123456789'

# a run longer than the room left: it wraps and goes on inserting
printf '\033[3;75Hthis wraps onto the next row'

# a run at column 1 and a run of one character
printf '\033[5;1Hfirst\033[5;40Hx'

# runs inside left/right margins
printf '\033[?69h\033[10;30s'
printf '\033[6;12Hmargins\033[6;28Hedge'
printf '\033[7;25Hpast the right margin'
printf '\033[?69l'

# a wide character pushed apart and off the edge by a run
printf '\033[8;77H\344\270\255\033[8;70Hpushed'
printf '\033[9;5H\344\270\255\033[9;4Hsplit'

# hyperlinked cells pushed off the edge by a run
printf '\033[10;70H\033]8;;http://example.org\033\\link\033]8;;\033\\'
printf '\033[10;60Hfar enough to push the link out'

# REP of a character, in insert mode
printf '\033[11;20H-\033[30b'
printf '\033[12;2H=\033[200b'

printf '\033[4l'
