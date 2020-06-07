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
printf ' rgb(255,1,153)                 rgb(254, 2, 152) \r\n'
printf ' rgb(253, 3, 151.0)             rgb(100\045,4\045,40\045)\r\n'
printf ' rgb(50\045, 0\045, 60\045)              rgb(255 0 153)\r\n'
printf ' rgb(254, 1, 150, 0)            rgba(254, 1, 150, 0)\r\n'
printf ' rgb(253, 2, 149, 1)            rgba(253, 2, 149, 1)\r\n'
printf ' rgb(252, 3, 148, 50\045)          rgba(252, 3, 148, 50\045)\r\n'
printf ' rgb(254 1 152 / 0)             rgba(254 1 152 / 0)\r\n'
printf ' rgb(253 2 151 / 100\045)          rgba(253 2 151 / 100\045) \r\n'
printf ' rgb(255, 0, 153.6, 1)          rgba(255, 0, 153.6, 1)\r\n'
printf ' rgb(1e2, .5e1, .5e0, +.25e2\045)  rgba(1e2, .5e1, .5e0, +.25e2\045)\r\n'
printf '\r\n'
printf ' Not Passing\r\n'
printf ' ===========\r\n'
printf ' rgb(100\045, 0, 60\045)      rgb(110\045, 0, 60\045)\r\n'
printf ' rgb(300, 0, 0)         rgba(0, 0, 0 >\r\n'
printf ' rgba(0, 0, 0, 1.2)     rgba(0, 0, 0, 102\045)\r\n'
printf ' rgba(0, 0, 0, 0, 0)\r\n'
printf '\r\n'
printf '(highlighted is where the mouse is when testing\r\n'
printf 'whether there is a color underneath)'

##
# Passing
##

## rgb(255,1,153)
printf '\033}tm;56;40\0'
printf '\033}tlc;1;2;14;2;255;1;153;255\0'

## rgb(254, 2, 152)
printf '\033}tm;291;36\0'
printf '\033}tlc;32;2;47;2;254;2;152;255\0'

## rgb(253, 3, 151.0)
printf '\033}tm;51;54\0'
 printf '\033}tlc;1;3;18;3;253;3;151;255\0'

## rgb(100%,4%,40%)
printf '\033}tm;296;52\0'
printf '\033}tlc;32;3;47;3;255;10;102;255\0'

## rgb(50%, 0%, 60%)
printf '\033}tm;37;67\0'
printf '\033}tlc;1;4;17;4;127;0;153;255\0'

## rgb(255 0 153)
printf '\033}tm;287;71\0'
printf '\033}tlc;32;4;45;4;255;0;153;255\0'

## rgb(254, 1, 150, 0)
printf '\033}tm;106;84\0'
printf '\033}tlc;1;5;19;5;254;1;150;0\0'

## rgba(254, 1, 150, 0)
printf '\033}tm;325;81\0'
printf '\033}tlc;32;5;51;5;254;1;150;0\0'

## rgb(253, 2, 149, 1)
printf '\033}tm;119;96\0'
printf '\033}tlc;1;6;19;6;253;2;149;255\0'

## rgba(253, 2, 149, 1)
printf '\033}tm;354;96\0'
printf '\033}tlc;32;6;51;6;253;2;149;255\0'

## rgb(252, 3, 148, 50%)
printf '\033}tm;48;115\0'
printf '\033}tlc;1;7;21;7;252;3;148;128\0'

## rgba(252, 3, 148, 50%)
printf '\033}tm;241;115\0'
printf '\033}tlc;32;7;53;7;252;3;148;128\0'

## rgb(254 1 152 / 0)
printf '\033}tm;79;129\0'
printf '\033}tlc;1;8;18;8;254;1;152;0\0'

## rgba(254 1 152 / 0)
printf '\033}tm;348;131\0'
printf '\033}tlc;32;8;50;8;254;1;152;0\0'

## rgb(253 2 151 / 100%)
printf '\033}tm;38;147\0'
printf '\033}tlc;1;9;21;9;253;2;151;255\0'

## rgba(253 2 151 / 100%)
printf '\033}tm;254;143\0'
printf '\033}tlc;32;9;53;9;253;2;151;255\0'

## rgb(255, 0, 153.6, 1)
printf '\033}tm;116;161\0'
printf '\033}tlc;1;10;21;10;255;0;154;255\0'

## rgba(255, 0, 153.6, 1)
printf '\033}tm;243;155\0'
printf '\033}tlc;32;10;53;10;255;0;154;255\0'

## rgb(1e2, .5e1, .5e0, +.25e2%)
printf '\033}tm;171;175\0'
printf '\033}tlc;1;11;29;11;100;5;1;64\0'

## rgba(1e2, .5e1, .5e0, +.25e2%)
printf '\033}tm;411;175\0'
printf '\033}tlc;32;11;61;11;100;5;1;64\0'

##
# Not passing
##

## rgb(100%, 0, 60%)
printf '\033}tm;51;233\0'
printf '\033}tlcn\0'

## rgb(110%, 0, 60%)
printf '\033}tm;206;234\0'
printf '\033}tlcn\0'

## rgb(300, 0, 0)
printf '\033}tm;39;250\0'
printf '\033}tlcn\0'

## rgba(0, 0, 0 >
printf '\033}tm;184;253\0'
printf '\033}tlcn\0'

## rgba(0, 0, 0, 1.2)
printf '\033}tm;18;263\0'
printf '\033}tlcn\0'
## rgba(0, 0, 0, 102%)
printf '\033}tm;190;265\0'
printf '\033}tlcn\0'

## rgba(0, 0, 0, 0, 0)
printf '\033}tm;22;280\0'
printf '\033}tlcn\0'
