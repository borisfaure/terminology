#!/bin/sh

# fill the screen with a distinct letter per row
printf '\033[H'
for c in A B C D E F G H I J K L M N O P Q R S T U V W X; do
   printf '%s0123456789abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456' "$c"
done

printf '\033[44m'

# more columns than room right of the cursor: blank to the right edge,
# never wrap onto the next row nor scroll
printf '\033[1;20r'
printf '\033[3;70H'
printf '\033[20\047}'

# with left/right margins: blank to the right margin only
printf '\033[?69h'
printf '\033[10;40s'
printf '\033[5;20H'
printf '\033[100\047}'

# the largest count must not take forever
printf '\033[?69l'
printf '\033[r'
printf '\033[22;79H'
printf '\033[2147483647\047}'
