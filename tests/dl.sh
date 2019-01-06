#!/bin/sh

# fill space
PL=1
for _ in $(seq 0 23); do
    PL=$((PL+1))
    if [ $PL -gt 9 ] ; then
        PL=1
    fi
    printf '%s' "$PL"
    for _ in $(seq 2 $PL); do
        printf '#'
    done
    PR=$((9 - PL))
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
    printf '%s' "$PR"
done

# set color
printf '\033[43;34;3m'

# move
printf '\033[1;10H'
# DL 0
printf '\033[0M1'
# move
printf '\033[3;10H'
# DL default
printf '\033[M2'
# move
printf '\033[5;10H'
# DL 2
printf '\033[2M3'


# set top/bottom margins:
printf '\033[8;14r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;75s'

# move
printf '\033[7;10H'
# outside margins (top), do nothing
printf '\033[M4'
# move
printf '\033[8;4H'
# outside margins(left), do nothing
printf '\033[M5'
# move
printf '\033[15;10H'
# outside margins(bottom), do nothing
printf '\033[M6'
# move
printf '\033[8;76H'
# outside margins(right), do nothing
printf '\033[M7'

# move
printf '\033[9;10H'
# DL 0
printf '\033[0M8'
# move
printf '\033[11;10H'
# DL default
printf '\033[M9'
# move
printf '\033[13;10H'
# DL 2
printf '\033[6M10'


# set top/bottom margins:
printf '\033[16;22r'
# restrict cursor
printf '\033[?6h'

# move
printf '\033[2;4H'
# DL 0
printf '\033[0M11'
# move
printf '\033[4;4H'
# DL default
printf '\033[M12'
# move
printf '\033[6;4H'
# DL 2
printf '\033[6M13'
