#!/bin/sh

# move to 0; 0
printf '\033[H'
# fill in left space
for _ in $(seq 0 23); do
    for _ in $(seq 0 3); do
        printf '\033[0;1m\'
        printf '\033[0;1m-'
        printf '\033[0;46;1;4m/'
        printf '\033[0;46;1;4m|'
        printf '\033[0;1;4;7m\\'
        printf '\033[0m~'
        printf '\033[0;1m_'
        printf '\033[0;31;7m>'
        printf '\033[0;31;4;7m^'
        printf '\033[0;1;7m<'
    done
    for _ in $(seq 0 3); do
        printf '\033[0m          '
    done
done

# move to 0; 0
printf '\033[H'

#set color
printf '\033[43;32;3m'

# set top/bottom margins:
printf '\033[3;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;75s'

# copy one char
printf '\033[3;3;3;3;;2;48;\044v'

# Copy rectangle
printf '\033[5;5;9;9;;8;45;\044v'

# invalid rectangles
printf '\033[5;5;4;9;;8;45;\044v'
printf '\033[5;5;9;4;;8;45;\044v'


# Copy rectangle with invalid page values
printf '\033[5;5;9;9;1337;14;55;1337\044v'

# Copy to part clipped
printf '\033[5;5;9;9;;22;78;\044v'

# Copy upon itself (full overlap)
printf '\033[5;5;9;9;;5;5;\044v'

# Copy upon itself (some overlap on the right)
printf '\033[5;5;9;9;;7;7;\044v'

# Copy upon itself (some overlap on the left)
printf '\033[15;5;19;9;;17;3;\044v'


# WITH MARGINS ENFORCED

# set top/bottom margins:
printf '\033[3;10r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;60s'

# restrict cursor
printf '\033[?6h'

printf '\033[5;21;9;25;;1;50;\044v'
# Copy rectangle to on margins
printf '\033[5;21;9;25;;6;54;\044v'
