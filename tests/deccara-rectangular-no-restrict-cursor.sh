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
        printf '\033[0;46;1;4;5m|'
        printf '\033[0;1;4;5;7m\\'
        printf '\033[0m~'
        printf '\033[0;1;5m_'
        printf '\033[0;31;5;7m>'
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

# set top/bottom margins:
printf '\033[5;20r'

# force rectangular modifications
printf '\033[2*x'

# column reset
printf '\033[1;10;80;15;0\044r'

# column reset
printf '\033[1;20;80;25;22;24;25;27\044r'

# reset bold/blink
printf '\033[1;30;80;35;22;25\044r'

# set bold/underline, remove blink/reverse
printf '\033[1;40;80;45;1;4;25;27\044r'

# set blink/reverse, remove bold/underline
printf '\033[1;50;80;55;5;7;22;24\044r'

# set all, remove all
printf '\033[1;60;80;65;1;4;5;7;0\044r'
