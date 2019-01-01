#!/bin/sh

# move to 0; 0
printf '\033[H'
# fill space
PL=0
for _ in $(seq 0 23); do
    PL=$((PL+1))
    if [ $PL -ge 9 ] ; then
        PL=0
    fi
    for _ in $(seq 1 $PL); do
        printf '#'
    done
    PR=$((10 - PL))
    for _ in $(seq 0 6); do
        printf '\033[0;1m\-'
        printf '\033[0;46;1;4m/'
        printf '\033[0;46;1;4m|'
        printf '\033[0;1;4;7m\\'
        printf '\033[0m~'
        printf '\033[0;1m_'
        printf '\033[0;31;7m>'
        printf '\033[0;31;4;7m^'
        printf '\033[0;1;7m<'
    done
    printf '\033[0m'
    for _ in $(seq 1 $PR); do
        printf '#'
    done
done

# move to 0; 0
printf '\033[H'

#set color
printf '\033[43;32;3m'

# move
printf '\033[3;60H'
# insert spaces
printf '\033[200@'

# move
printf '\033[4;12H'
# insert spaces
printf '\033[13@'

# set top/bottom margins:
printf '\033[3;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;75s'

# move
printf '\033[20;70H'
# insert spaces
printf '\033[200@'

# move
printf '\033[0;70H'
# insert spaces
printf '\033[200@'

# move
printf '\033[2;4H'
# do not insert spaces, out of left/right margins
printf '\033[200@'

# move
printf '\033[2;76H'
# do not insert spaces, out of left/right margins
printf '\033[200@'


# move
printf '\033[7;5H'
# insert spaces
printf '\033[20@'

# move
printf '\033[7;75H'
# insert spaces
printf '\033[20@'

# move
printf '\033[8;8H'
# insert 1 space
printf '\033[0@'

# WITH MARGINS ENFORCED

# set top/bottom margins:
printf '\033[3;10r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;60s'

# restrict cursor
printf '\033[?6h'


# move
printf '\033[8;50H'
# insert spaces
printf '\033[200@'
