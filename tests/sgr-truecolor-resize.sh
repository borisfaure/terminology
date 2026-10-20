#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# fill the screen with distinct RGB cells, then reflow and check that the
# RGB values move with their cells
printf '\033[38;2;255;0;0mA\033[38;2;0;255;0mB\033[38;2;0;0;255mC\033[38;2;255;255;0mD\033[38;2;0;255;255mE\033[38;2;255;0;255mF'
printf '\033[48;2;10;20;30m \033[48;2;40;50;60m \033[48;2;70;80;90m \033[48;2;100;110;120m \033[48;2;130;140;150m \033[48;2;160;170;180m \033[48;2;190;200;210m \033[48;2;220;230;240m'

# force render
printf '\033}tr\0'

# resize window to half width (shall display the same)
printf '\033[8;;40;t'

# force render
printf '\033}tr\0'
