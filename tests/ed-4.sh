#!/bin/sh


# fill space
for V in $(seq 0 50); do
   printf '%s\n' "$V"
done

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

# move
printf '\033[10;30H'

# ED 4 nothing
printf '\033[4J'
