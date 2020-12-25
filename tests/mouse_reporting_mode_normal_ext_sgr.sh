#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# char width: 7
# char height: 15

# resize window
#resize window to 180-chars width
printf '\033[8;;180;t'
# force render
printf '\033}tr\0'
test_sleep 0.2

# fill space with E
printf '\033#8'

# set color
printf '\033[46;31;3m'

# positions used are;
# - 200;130 -> 9;29
# - 480;130 -> 9;69
# - 1230;130 -> 9;176
# indicate the positions to help testing
printf '\033[9;29H#'
printf '\033[9;69H#'
printf '\033[9;95H@' # after that one, MOUSE_X10 is not working
printf '\033[9;176H#'

# set mouse mode - normal - SGR reporting
printf '\033[?1006l'
printf '\033[?1015l'
printf '\033[?1000h'
printf '\033[?1006h'
printf '\033[H'

## LEFT CLICK
# should print ^[[<0;29;9M^[[<0;69;9m
# move cursor
printf '\033[1H'
# mouse down
printf '\033}td;200;130;1;0;0\0'
# mouse move
printf '\033}tm;340;130;0\0'
printf '\033}tm;480;130;0\0'
# mouse up
printf '\033}tu;480;130;1;0;0\0'
# mouse move back
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'

# mouse click -> ^[[<0;29;9M^[[<0;69;9m
printf '\033}td;480;130;1;0;0\0'
# mouse move
printf '\033}tm;657;130;0\0'
# mouse move
printf '\033}tm;664;130;0\0'
# mouse move
printf '\033}tm;671;130;0\0'
# mouse move
printf '\033}tm;1230;130;0\0'
# mouse up
printf '\033}tu;1230;130;1;0;0\0'
#not pressed
# mouse move
printf '\033}tm;671;130;0\0'
# mouse move
printf '\033}tm;664;130;0\0'
# mouse move
printf '\033}tm;657;130;0\0'
# force render
printf '\033}tr\0'
test_sleep 0.2

## RIGHT CLICK
# should print ^[[<2;29;9M;^[[<2;69;9m
# move cursor
printf '\033[2H'
# mouse down
printf '\033}td;200;130;3;0;0\0'
# mouse move
printf '\033}tm;340;130;0\0'
printf '\033}tm;480;130;0\0'
# mouse up
printf '\033}tu;480;130;3;0;0\0'
# mouse move back
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'
# force render
printf '\033}tr\0'
test_sleep 0.2

## MIDDLE CLICK
# should print ^[[<1;29;9M^[[<1;69;9m
# move cursor
printf '\033[3H'
# mouse down
printf '\033}td;200;130;2;0;0\0'
# mouse move
printf '\033}tm;480;130;0\0'
# mouse up
printf '\033}tu;480;130;2;0;0\0'
# mouse move back
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'
# force render
printf '\033}tr\0'
test_sleep 0.2

## WHEEL
# prints ^[[<64;29;9M^[[<65;29;9M^[[<64;69;9M^[[<65;69;9M
# move cursor
printf '\033[4H'
# wheel up/down
printf '\033}tw;200;130;1;1;0\0'
printf '\033}tw;200;130;0;1;0\0'
printf '\033}tw;480;130;1;1;0\0'
printf '\033}tw;480;130;0;1;0\0'
# force render
printf '\033}tr\0'
test_sleep 0.2


##
# Same with Alt
##
printf '\033[6HWith Alt:'

## LEFT CLICK
# should print ^[[<8;29;9M^[[<8;69;9m
# move cursor
printf '\033[7H'
# mouse down
printf '\033}td;200;130;1;1;0\0'
# mouse move
printf '\033}tm;480;130;1\0'
# mouse up
printf '\033}tu;480;130;1;1;0\0'
# mouse move back
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
test_sleep 0.2


## RIGHT CLICK
# should print ^[[<10;29;9M^[[<10;69;9m
# move cursor
printf '\033[8H'
# mouse down
printf '\033}td;200;130;3;1;0\0'
# mouse move
printf '\033}tm;480;130;1\0'
# mouse up
printf '\033}tu;480;130;3;1;0\0'
# mouse move back
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
test_sleep 0.2


## MIDDLE CLICK
# should print ^[[<9;29;9M^[[<9;69;9m
# move cursor
printf '\033[9H'
# mouse down
printf '\033}td;200;130;2;1;0\0'
# mouse move
printf '\033}tm;480;130;1\0'
# mouse up
printf '\033}tu;480;130;2;1;0\0'
# mouse move back
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
test_sleep 0.2

## WHEEL
# prints ^[[<72;29;9M^[[<73;29;9M^[[<72;69;9M^[[<73;69;9M
# move cursor
printf '\033[10H'
# wheel up/down
printf '\033}tw;200;130;1;1;1\0'
printf '\033}tw;200;130;0;1;1\0'
printf '\033}tw;480;130;1;1;1\0'
printf '\033}tw;480;130;0;1;1\0'
# force render
printf '\033}tr\0'
test_sleep 0.2
printf '\033[14H'
