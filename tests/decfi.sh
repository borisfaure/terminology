#!/bin/sh

# move to 0; 0
printf '\033[H'
# fill space
for _ in $(seq 0 23); do
   for _ in $(seq 0 7); do
      printf '\-/|\\~_>^<'
   done
done

# move to 0; 0
printf '\033[H'

#set color
printf '\033[46;31;3m'

printf '   format is:\n   \-/|\\~_>^<'

# move to
printf '\033[3;81H'
# decfi, insert column
printf '\033\071a'

# set top/bottom margins:
printf '\033[5;22r'

# move to
printf '\033[4;81H'
# decfi, not within top/bottom margin, do nothing
printf '\033\071b'
# move to 3; 0
printf '\033[6;81H'
# decfi, insert column within margins
printf 'C\033\071c'


# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'

# move
printf '\033[7;81H'
# decfi, do nothing
printf 'D\033\071d'

# set right margin only
printf '\033[;14s'

# move
printf '\033[8;81H'
# decfi, do nothing. Xterm seems change wrapping here. I don't see why
printf 'E\033\071e'


# set left/right margins:
printf '\033[5;14s'
# move to 0; 0
printf '\033[10;5H'
# fill space
for Y in $(seq 10 20); do
    # move to 0; 0
    printf '\033[%s;5H' "$Y"
    printf '\-/|\\~_>^<'
done

# move
printf '\033[11;81H'
# decfi, do nothing. Xterm seems change wrapping here. I don't see why
# @xtermbug
printf 'f\033\071g'

# move
printf '\033[12;12H'
#decfi, outside right margin, cursor forward
printf 'h\033\071i'

# move
printf '\033[13;3H'
#decfi, outside right margin, cursor forward
printf '\033\071j'

# move
printf '\033[14;14H'
#decfi, on left margin, insert column
printf '\033\071k'
