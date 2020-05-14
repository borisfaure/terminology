#!/bin/sh

# char width: 7
# char height: 15

# set color
printf '\033[46;31;3m'

# clear screen
printf '\033[2J'

# move to 0; 0
printf '\033[0;0H'

printf ' Passing                          Not Passing\r\n'
printf ' =======                          ===========\r\n'
printf ' #123456                          ##ff00ff \r\n'
printf ' #ffffff                          #ff00ff# \r\n'
printf ' #f9472aa8                        #fgfgfg \r\n'
printf ' #fFaAbBcC                        #fffffffff \r\n'
printf ' #0af                             #ff \r\n'
printf ' #a098                            ff00ff\r\n'
printf '                                  >ff00ff\r\n'

printf '\r\n(highlighted is where the mouse is when testing'
printf '\r\n whether there is a color underneath)'

##
# Passing
##

##  #123456
printf '\033}tm;30;40\0'
printf '\033}tlc;1;2;7;2;18;52;86;255\0'

##  #ffffff
printf '\033}tm;48;52\0'
printf '\033}tlc;1;3;7;3;255;255;255;255\0'

##  #f9472aa8
printf '\033}tm;12;67\0'
printf '\033}tlc;1;4;9;4;249;71;42;168\0'

##  #fFaAbBcC
printf '\033}tm;26;82\0'
printf '\033}tlc;1;5;9;5;255;170;187;204\0'

##  #0af
printf '\033}tm;35;98\0'
printf '\033}tlc;1;6;4;6;0;160;240;255\0'

##  #a098
printf '\033}tm;16;117\0'
printf '\033}tlc;1;7;5;7;160;0;144;128\0'


##
# Not passing
##

##
printf '\033}tm;195;65\0'
printf '\033}tlcn\0'

##  ##ff00ff
printf '\033}tm;263;38\0'
printf '\033}tlcn\0'

##  #ff00ff#
printf '\033}tm;278;55\0'
printf '\033}tlcn\0'

##  #fgfgfg
printf '\033}tm;248;71\0'
printf '\033}tlcn\0'

##  #fffffffff
printf '\033}tm;274;85\0'
printf '\033}tlcn\0'

##  #ff
printf '\033}tm;249;100\0'
printf '\033}tlcn\0'

##  ff00ff
printf '\033}tm;263;113\0'
printf '\033}tlcn\0'

##  >ff00ff
printf '\033}tm;252;131\0'
printf '\033}tlcn\0'
