#!/bin/sh

# rectangle operations with the bottom and right arguments at INT32_MAX
# while the cursor is restricted to the margins: adding the margins must
# not overflow them past the screen clamp

# fill space with E
printf '\033#8'
# set top/bottom margins
printf '\033[5;20r'
# allow and set left/right margins
printf '\033[?69h\033[10;70s'
# restrict cursor
printf '\033[?6h'

for mode in 1 2; do
   # DECSACE: stream, then rectangle
   printf '\033[%s*x' "$mode"
   # DECFRA
   printf '\033[42;3;3;2147483647;2147483647$x'
   # DECCARA
   printf '\033[3;3;2147483647;2147483647;1;4$r'
   # DECRARA
   printf '\033[4;4;2147483647;2147483647;7$t'
   # DECERA
   printf '\033[8;8;2147483647;2147483647$z'
done

# bottom margin at the last line: a bottom argument one past the margin
# area must stop at the last line, not wrap onto the first one
printf '\033[?69l\033[5;24r\033[1*x'
printf '\033[43;1;1;21;10$x'
printf '\033[2*x'
printf '\033[44;1;20;21;30$x'

printf '\033[?6l\033[r\033[1*x'
