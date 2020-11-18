#!/bin/bash
#
#   This file echoes a bunch of color codes to the terminal to demonstrate
#   what's available.  Each line is the color code of one foreground color,
#   out of 33 (default + normal + bright + faint + brightfaint), followed by a
#   test use of that color on all nine background colors (default + 8
#   escapes).
#

T='gYw'   # The test text
T='g█$'   # The test text

echo -e "\n                   40m     41m     42m     43m\
     44m     45m     46m     47m    100m    101m    102m    103m    104m\
     105m    106m    107m";

for FGs in '      m' '     1m'  '     2m'  '1;2;30m' \
   '    30m' '  1;30m' '    90m' '  2;30m' '1;2;30m' \
   '    31m' '  1;31m' '    91m' '  2;31m' '1;2;31m' \
   '    32m' '  1;32m' '    92m' '  2;32m' '1;2;32m' \
   '    33m' '  1;33m' '    93m' '  2;33m' '1;2;33m' \
   '    34m' '  1;34m' '    94m' '  2;34m' '1;2;34m' \
   '    35m' '  1;35m' '    95m' '  2;35m' '1;2;35m' \
   '    36m' '  1;36m' '    96m' '  2;36m' '1;2;36m' \
   '    37m' '  1;37m' '    97m' '  2;37m' '1;2;37m';
  do FG=${FGs// /}
  echo -en " $FGs \033[$FG  $T  "
  for BG in 40m 41m 42m 43m 44m 45m 46m 47m 100m 101m 102m 103m 104m 105m 106m 107m;
    do echo -en "$EINS \033[$FG\033[$BG  $T  \033[0m";
  done
  echo;
done
echo
