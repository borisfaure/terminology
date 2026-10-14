#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

modes="1 2 3 4 5 6 7 8 9 12 20 25 40 47 66 67 69 1000 1002 1003 1004 \
1005 1006 1015 1016 1034 1047 1048 1049 2004 2026 2027 2031 7727 7728"

# initial state
for m in $modes; do
  printf '\033[?%s\044p' "$m"
done

# set what can be set, query again
printf '\033[?1h\033[?5h\033[?6h\033[?7l\033[?8l\033[?20h\033[?25l'
printf '\033[?67h\033[?69h\033[?2004h\033[?7727h\033[?7728h'
printf '\033[?1004h'
for m in 1 5 6 7 8 20 25 67 69 1004 2004 7727 7728; do
  printf '\033[?%s\044p' "$m"
done

# mouse tracking modes are exclusive, so are the encodings
printf '\033[?1002h\033[?1015h'
for m in 9 1000 1002 1003 1005 1006 1015; do
  printf '\033[?%s\044p' "$m"
done
printf '\033[?1003h\033[?1006h'
for m in 1000 1002 1003 1006 1015; do
  printf '\033[?%s\044p' "$m"
done

# the three alternate screen modes all read the same buffer flag
printf '\033[?1049h'
printf '\033[?47\044p\033[?1047\044p\033[?1049\044p'
printf '\033[?1049l'
printf '\033[?47\044p\033[?1047\044p\033[?1049\044p'

# synchronized output, inside and outside a frame
printf '\033[?2026h\033[?2026\044p\033[?2026l\033[?2026\044p'

# unknown modes are echoed with 0
printf '\033[?0\044p\033[?9999\044p\033[?65535\044p'
# only the first parameter is answered
printf '\033[?1;7\044p'
# no reply: missing mode, overflowing mode, wrong private marker
printf '\033[?\044p'
printf '\033[?99999999999999999999\044p'
printf '\033[>4\044p'

# leave the terminal tidy
printf '\033[?1l\033[?5l\033[?6l\033[?7h\033[?8h\033[?20l\033[?25h'
printf '\033[?1004l\033[?1003l\033[?1006l'
