#!/bin/sh

# char width: 7
# char height: 15

# set color
printf '\033[46;31;3m'

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

printf ' Passing\r\n'
printf ' =======\r\n'
printf ' color:   51 153 255  32;\r\n'
printf ' color2: 244  99  93 127;\r\n'
printf ' color3: 149 181  16 234;\r\n'
printf ' color: #3399ff20;\r\n'
printf ' color\t:\t244\t99\t93\t127;\r\n'
printf ' type: RECT;color: 51 153 255 32;\r\n'
printf '\r\n'
printf ' Not Passing\r\n'
printf ' ===========\r\n'
printf ' color: COL;\r\n'
printf '\r\n'
printf '(highlighted is where the mouse is when testing\r\n'
printf 'whether there is a color underneath)'

##
# Passing
##

## color:   51 153 255  32;
printf '\033}tm;81;39\0'
printf '\033}tlc;1;2;23;2;51;153;255;32\0'
printf '\033}tm;20;37\0'
printf '\033}tlc;1;2;23;2;51;153;255;32\0'

##   color2: 244  99  93 127;
printf '\033}tm;36;52\0'
printf '\033}tlc;1;3;23;3;244;99;93;127\0'
printf '\033}tm;136;52\0'
printf '\033}tlc;1;3;23;3;244;99;93;127\0'

## color3: 149 181  16 234;
printf '\033}tm;163;70\0'
printf '\033}tlc;1;4;23;4;149;181;16;234\0'
printf '\033}tm;31;73\0'
printf '\033}tlc;1;4;23;4;149;181;16;234\0'

## color: #3399ff20;
printf '\033}tm;107;82\0'
printf '\033}tlc;8;5;16;5;51;153;255;32\0'
printf '\033}tm;18;82\0'
printf '\033}tlc;1;5;16;5;51;153;255;32\0'


## color\t:\t244\t99\93\t127;
printf '\033}tm;33;102\0'
printf '\033}tlc;1;6;42;6;244;99;93;127\0'
printf '\033}tm;178;98\0'
printf '\033}tlc;1;6;42;6;244;99;93;127\0'

## type: RECT;color: 51 153 255 32;
printf '\033}tm;199;112\0'
printf '\033}tlc;12;7;31;7;51;153;255;32\0'
printf '\033}tm;98;118\0'
printf '\033}tlc;12;7;31;7;51;153;255;32\0'



##
# Not passing
##

## color: COL;
printf '\033}tm;68;177\0'
printf '\033}tlcn\0'
printf '\033}tm;22;176\0'
printf '\033}tlcn\0'

