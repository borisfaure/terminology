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

# move to 0; 0
printf '\033[H'

#set color
printf '\033[46;31;3m'

printf 'format is:\n\-/|\\~_>^<'

# move to
printf '\033[3;0H'
# decbi, insert column
printf '\033\066a'

# set top/bottom margins:
printf '\033[5;22r'

# move to
printf '\033[4;0H'
# decbi, not within top/right margin, do nothing
printf '\033\066b'
# move to 3; 0
printf '\033[6;0H'
# decbi, insert column within margins
printf '\033\066c'


# set top/bottom margins:
printf '\033[10;20r'
# allow left/right margins
printf '\033[?69h'

# move
printf '\033[7;0H'
# decbi, do nothing
printf '\033\066d'

# set right margin only
printf '\033[;14s'

# move
printf '\033[8;0H'
# decbi, do nothing
printf '\033\066e'


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
printf '\033[11;0H'
#decbi, outside left margin, cursor back
printf 'f\033\066g'

# move
printf '\033[12;12H'
#decbi, outside left margin, cursor back
printf 'h\033\066i'

# move
printf '\033[13;3H'
#decbi, outside left margin, cursor back
printf '\033\066j'

# move
printf '\033[14;5H'
#decbi, on left margin, insert column
printf '\033\066k'
