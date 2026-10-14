#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

modes="1 5 6 7 8 25 67 69 1000 1006 2004 7727 7728"
set_all()
{
  printf '\033[?1h\033[?5h\033[?6h\033[?7l\033[?8l\033[?25l\033[?67h'
  printf '\033[?69h\033[?1000h\033[?1006h\033[?2004h\033[?7727h\033[?7728h'
  printf '\033[4h'
}
query_all()
{
  for m in $modes; do
    printf '\033[?%s\044p' "$m"
  done
  printf '\033[4\044p'
}

set_all
query_all
# DECSTR
printf '\033[!p'
query_all
set_all
# RIS
printf '\033c'
query_all
