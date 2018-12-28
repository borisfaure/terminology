#!/bin/sh

# move to 0; 0
printf '\033[H'
# fill space
for _ in $(seq 0 23); do
   for _ in $(seq 0 7); do
      printf '\-/|\\~_>^<'
   done
done
# move to 0; 0
printf '\033[H'

#set color
printf '\033[46;31;3m'

# move to 2; 1
printf '\033[2;1H'
# remove one char
printf '\033[P'
# move to 3; 1
printf '\033[3;1H'
# remove 8 chars
printf '\033[8P'
# move to 4; 1
printf '\033[4;1H'
# remove too many chars
printf '\033[888P'


# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;22s'

# outside margins
# move to 5; 2
printf '\033[5;5H'
# remove one char
printf '\033[P'
# move to 6; 2
printf '\033[6;2H'
# remove 8 chars
printf '\033[8P'
# move to 7; 2
printf '\033[7;2H'
# remove too many chars
printf '\033[888P'

# outside margins
# move to 8; 22
printf '\033[8;22H'
# remove one char
printf '\033[P'
# move to 9; 2
printf '\033[9;27H'
# remove 8 chars
printf '\033[8P'
# move to 10; 2
printf '\033[10;27H'
# remove too many chars
printf '\033[888P'

# inside margins
# move to 11; 2
printf '\033[11;7H'
# remove one char
printf '\033[P'
# move to 12; 2
printf '\033[12;7H'
# remove 8 chars
printf '\033[8P'
# move to 13; 2
printf '\033[13;7H'
# remove too many chars
printf '\033[888P'

# restrict cursor
printf '\033[?6h'

# inside margins
# move to 14; 2
printf '\033[14;2H'
# remove one char
printf '\033[P'
# move to 15; 2
printf '\033[15;2H'
# remove 8 chars
printf '\033[8P'
# move to 16; 2
printf '\033[16;2H'
# remove too many chars
printf '\033[888P'

#remove margins
printf '\033[?69l'
# move to 20; 41
printf '\033[20;41H'
# remove 40 chars
printf '\033[40P'
