#!/bin/sh

# char width: 7
# char height: 15

# resize window
#resize window to 180-chars width
printf '\033[8;;180;t'
# force render
printf '\033}tr\0'
sleep 0.2

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

# set mouse mode - normal button move while pressed - SGR reporting
printf '\033[?1005l'
printf '\033[?1015l'
printf '\033[?1003h'
printf '\033[?1006h'
printf '\033[H'

## LEFT CLICK
# should print
# move cursor
printf '\033[1H'
# mouse down -> ^[[<0;29;9M
printf '\033}td;200;130;1;0;0\0'
# mouse move -> ^[[<32;49;9^[[<32;69;9
printf '\033}tm;340;130;0\0'
printf '\033}tm;480;130;0\0'
# mouse up  -> ^[[<0;69;9m
printf '\033}tu;480;130;1;0;0\0'
# mouse move back -> ^[[<35;49;9M^[[<35;29;9M
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'

# mouse click -> ^[[<0;69;9M
printf '\033}td;480;130;1;0;0\0'
# mouse move -> ^[[<32;94;9m
printf '\033}tm;657;130;0\0'
# mouse move -> ^[[<32;95;9m
printf '\033}tm;664;130;0\0'
# mouse move -> -> ^[[<32;96;9m
printf '\033}tm;671;130;0\0'
# mouse move -> ^[[<32;176;9m
printf '\033}tm;1230;130;0\0'
# mouse up -> ^[[<0;176;9m
printf '\033}tu;1230;130;1;0;0\0'
#not pressed
# mouse move -> ^[[<35;96;9M
printf '\033}tm;671;130;0\0'
# mouse move -> ^[[<35;95;9M
printf '\033}tm;664;130;0\0'
# mouse move -> ^[[<35;94;9M
printf '\033}tm;657;130;0\0'
# force render
printf '\033}tr\0'
sleep 0.2

## RIGHT CLICK
# should print
# move cursor
printf '\033[2H'
# mouse down -> ^[[<2;29;9M
printf '\033}td;200;130;3;0;0\0'
# mouse move ^[[<34;49;9M^[[<34;69;9M
printf '\033}tm;340;130;0\0'
printf '\033}tm;480;130;0\0'
# mouse up -> ^[[<2;69;9m
printf '\033}tu;480;130;3;0;0\0'
# mouse move back -> ^[[<35;49;9M^[[<35;29;9M
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'
# force render
printf '\033}tr\0'
sleep 0.2

## MIDDLE CLICK
# should print
# move cursor
printf '\033[3H'
# mouse down -> ^[[<1;29;9M
printf '\033}td;200;130;2;0;0\0'
# mouse move -> ^[[<33;69;9M
printf '\033}tm;480;130;0\0'
# mouse up -> ^[[<1;69;9m
printf '\033}tu;480;130;2;0;0\0'
# mouse move back -> ^[[<35;49;9M^[[<35;29;9M
printf '\033}tm;340;130;0\0'
printf '\033}tm;200;130;0\0'
# force render
printf '\033}tr\0'
sleep 0.2

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
sleep 0.2


##
# Same with Alt
##
printf '\033[6HWith Alt:'

## LEFT CLICK
# should print
# move cursor
printf '\033[7H'
# mouse down -> ^[[<8;29;9M
printf '\033}td;200;130;1;1;0\0'
# mouse move -> ^[[<40;69;9m
printf '\033}tm;480;130;1\0'
# mouse up -> ^[[<8;69;9m
printf '\033}tu;480;130;1;1;0\0'
# mouse move back -> ^[[<43;49;9M^[[<43;29;9M
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
sleep 0.2


## RIGHT CLICK
# should print
# move cursor
printf '\033[8H'
# mouse down -> ^[[<10;29;9M
printf '\033}td;200;130;3;1;0\0'
# mouse move -> ^[[<42;69;9M
printf '\033}tm;480;130;1\0'
# mouse up -> ^[[<10;69;9m
printf '\033}tu;480;130;3;1;0\0'
# mouse move back -> ^[[<43;49;9M^[[<43;29;9M
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
sleep 0.2


## MIDDLE CLICK
# should print
# move cursor
printf '\033[9H'
# mouse down -> ^[[<9;29;9M
printf '\033}td;200;130;2;1;0\0'
# mouse move -> ^[[<41;29;9M
printf '\033}tm;480;130;1\0'
# mouse up -> ^[[<9;69;9m
printf '\033}tu;480;130;2;1;0\0'
# mouse move back -> ^[[<43;49;9M^[[<43;29;9M
printf '\033}tm;340;130;1\0'
printf '\033}tm;200;130;1\0'
# force render
printf '\033}tr\0'
sleep 0.2

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
sleep 0.2
printf '\033[14H'
