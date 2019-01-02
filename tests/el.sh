#!/bin/sh

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


# set color
printf '\033[43;32;3m'

# set top/bottom margins:
printf '\033[3;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;75s'

# restrict cursor
printf '\033[?6h'

printf '\033[3;30H#'
# move
printf '\033[4;30H#\033[4;30H'
# EL 0 -> Erase Line Right
printf '\033[0K'
# move
printf '\033[5;30H#\033[5;30H'
# EL 0 (default) -> Erase Line Right
printf '\033[K'

printf '\033[6;30H#'

# move
printf '\033[7;30H#\033[7;30H'
# EL 1 -> Erase Line Left
printf '\033[1K'

printf '\033[8;30H#'

# move
printf '\033[9;30H#\033[9;30H'
# EL 2 -> Erase Complete Line
printf '\033[2K'

printf '\033[10;30H#'

# move
printf '\033[11;30H#\033[11;30H'
# EL 3 -> Nothing
printf '\033[3K'
