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

# set top/bottom margins:
printf '\033[3;20r'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[5;75s'


#
# INSERT
#

# move to
printf '\033[2;2H'
# insert column, outside margin, do nothing
printf '\033[\047}'
# move to
printf '\033[2;8H'
# insert column, outside margin, do nothing
printf '\033[\047}'
# move to
printf '\033[5;2H'
# insert column, outside margin, do nothing
printf '\033[\047}'


# move to
printf '\033[10;12H'
# insert column
printf '\033[\047}'

# move to
printf '\033[10;17H'
# insert 0 column (but 1 actually)
printf '\033[0\047}'

# move to
printf '\033[10;21H'
# insert 3 columns
printf '\033[3\047}'


#
# DELETE
#

# move to
printf '\033[2;78H'
# insert column, outside margin, do nothing
printf '\033[\047~'
# move to
printf '\033[10;78H'
# insert column, outside margin, do nothing
printf '\033[\047~'
# move to
printf '\033[2;60H'
# insert column, outside margin, do nothing
printf '\033[\047~'

# move to
printf '\033[10;50H'
# delete column
printf '\033[\047~'

# move to
printf '\033[10;55H'
# delete 0 column (but 1 actually)
printf '\033[0\047~'

# move to
printf '\033[10;60H'
# delete 3 columns
printf '\033[3\047~'

