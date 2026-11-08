#!/bin/sh

# Hyperlinked cells moved around by scrolls and line insertions. A link
# whose last cell goes away frees its slot for the next link, so the ids
# of the links opened at the end check the refcounts kept along the way.

# clear screen, links on rows 1 to 8, each one in two runs
printf '\033[2J'
for i in 1 2 3 4 5 6 7 8; do
  printf '\033[%d;1H\033]8;;http://row%d.example\033\\link %d' $i $i $i
  printf '\033]8;;\033\\ plain \033]8;;http://row%d.example\033\\again' $i
  printf '\033]8;;\033\\'
done

# scroll region rows 2 to 6: two of its links scroll out, row 6 is blank
printf '\033[2;6r\033[6;1H\n\n'

# insert and delete lines inside the region
printf '\033[3;1H\033[L\033[4;1H\033[2M'

# left and right margins: scroll columns 3 to 9 only, splitting the
# runs of row 4 and scrolling out a link held by columns 4 to 6 only
printf '\033[3;4H\033]8;;http://mid.example\033\\mid\033]8;;\033\\'
printf '\033[?69h\033[3;9s\033[6;3H\n\n\n'
printf '\033[?69l\033[r'

# new links take the freed slots, lowest first
for i in 1 2 3 4 5 6 7 8 9 10; do
  printf '\033[%d;40H\033]8;;http://new%d.example\033\\new %d' $((i + 10)) $i $i
  printf '\033]8;;\033\\'
done
