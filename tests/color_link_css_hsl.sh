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
printf ' hsl(270,60\045,70\045)              hsl(270, 60\045, 70\045)\r\n'
printf ' hsl(270 60\045 70\045)              hsl(270deg, 60\045, 70\045)\r\n'
printf ' hsl(4.71239rad, 60\045, 70\045)     hsl(300grad, 60\045, 70\045)\r\n'
printf ' hsl(.75turn, 60\045, 70\045)        hsl(270, 60\045, 50\045, .15)\r\n'
printf ' hsl(270, 60\045, 50\045, 15\045)       hsl(270 60\045 50\045 / .15)\r\n'
printf ' hsl(270 60\045 50\045 / 15\045)        hsla(240, 100\045, 50\045, .05)\r\n'
printf ' hsla(240, 100\045, 50\045, .4)      hsla(600, 100\045, 50\045, .7)\r\n'
printf ' hsla(240, 100\045, 50\045, 1)       hsla(240 100\045 50\045 / .05)\r\n'
printf ' hsla(240 100\045 50\045 / 5\045)\r\n'
printf '\r\n'
printf ' Not Passing\r\n'
printf ' ===========\r\n'
printf ' hsl(100\045, 0\045, 60\045)        hsl(900, 0\045, 160\045)\r\n'
printf ' hsl(0, 1000\045, 0\045)         hsla(0, 0\045, 0\045 >\r\n'
printf ' hsla(0, 0, 0, 1.2)        hsla(0, 0, 0, 102\045)\r\n'
printf ' hsla(0, 0\045, 0\045, 0\045, 0\045)\r\n'
printf '\r\n'
printf '(highlighted is where the mouse is when testing\r\n'
printf 'whether there is a color underneath)'

##
# Passing
##
## hsl(270,60%,70%)
printf '\033}tm;76;36\0'
printf '\033}tlc;1;2;16;2;224;135;132;255\0'

## hsl(270, 60%, 70%)
printf '\033}tm;294;34\0'
printf '\033}tlc;31;2;48;2;224;135;132;255\0'

## hsl(270 60% 70%)
printf '\033}tm;32;56\0'
printf '\033}tlc;1;3;16;3;224;135;132;255\0'

## hsl(270deg, 60%, 70%)
printf '\033}tm;311;56\0'
printf '\033}tlc;31;3;51;3;224;135;132;255\0'

## hsl(4.71239rad, 60%, 70%)
printf '\033}tm;89;67\0'
printf '\033}tlc;1;4;25;4;224;135;132;255\0'

## hsl(300grad, 60%, 70%)
printf '\033}tm;328;68\0'
printf '\033}tlc;31;4;52;4;224;135;132;255\0'

## hsl(.75turn, 60%, 70%)
printf '\033}tm;14;84\0'
printf '\033}tlc;1;5;22;5;224;135;132;255\0'

## hsl(270, 60%, 50%, .15)
printf '\033}tm;226;85\0'
printf '\033}tlc;31;5;53;5;204;55;51;38\0'

## hsl(270, 60%, 50%, 15%)
printf '\033}tm;107;99\0'
printf '\033}tlc;1;6;23;6;204;55;51;38\0'

## hsl(270 60% 50% / .15)
printf '\033}tm;312;99\0'
printf '\033}tlc;31;6;52;6;204;55;51;38\0'

## hsl(270 60% 50% / 15%)
printf '\033}tm;44;111\0'
printf '\033}tlc;1;7;22;7;204;55;51;38\0'

## hsla(240, 100%, 50%, .05)
printf '\033}tm;238;114\0'
printf '\033}tlc;31;7;55;7;255;7;0;13\0'

## hsla(240, 100%, 50%, .4)
printf '\033}tm;95;129\0'
printf '\033}tlc;1;8;24;8;255;7;0;102\0'

## hsla(600, 100%, 50%, .7)
printf '\033}tm;336;127\0'
printf '\033}tlc;31;8;54;8;255;7;0;179\0'

## hsla(240, 100%, 50%, 1)
printf '\033}tm;27;142\0'
printf '\033}tlc;1;9;23;9;255;7;0;255\0'

## hsla(240 100% 50% / .05)
printf '\033}tm;237;142\0'
printf '\033}tlc;31;9;54;9;255;7;0;13\0'

## hsla(240 100% 50% / 5%)
printf '\033}tm;153;158\0'
printf '\033}tlc;1;10;23;10;255;7;0;13\0'


##
# Not passing
##

## hsl(100%, 0%, 60%)
printf '\033}tm;52;220\0'
printf '\033}tlcn\0'

## hsl(900, 0%, 160%)
printf '\033}tm;295;219\0'
printf '\033}tlcn\0'

## hsl(0, 1000%, 0%)
printf '\033}tm;80;234\0'
printf '\033}tlcn\0'

## hsla(0, 0%, 0% >
printf '\033}tm;201;235\0'
printf '\033}tlcn\0'

## hsla(0, 0, 0, 1.2)
printf '\033}tm;91;251\0'
printf '\033}tlcn\0'

## hsla(0, 0, 0, 102%)
printf '\033}tm;198;249\0'
printf '\033}tlcn\0'

## hsla(0, 0%, 0%, 0%, 0%)
printf '\033}tm;11;265\0'
printf '\033}tlcn\0'
