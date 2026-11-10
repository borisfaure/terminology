#!/bin/sh

# clear screen
printf '\033[2J\033[H'

# left/right margins on columns 30-60, scroll region on rows 10-20
printf '\033[?69h\033[30;60s\033[10;20r'

# shrink below both: the margins must not survive past the new edges
printf '\033[8;5;10t'

printf '\033[3;1Hhello world'
printf '\033[2L\033[2M\033[S\033[T'
printf '\033[1;1H\0339'
printf '\033[1;1;5;10$x'
printf '\033[1;1;5;10;7$r'

# grow back: the text has to land in a full-width scroll region
printf '\033[8;24;80t'
printf '\033[24;1Hbottom line\r\n'
printf '\033[1;80Hx'
