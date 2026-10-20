#!/bin/sh

printf '\033[2J\033[H'
# every form, fg and bg
printf '\033[38;2;1;2;3mA\033[48;2;4;5;6mB\033[m'
printf '\033[38:2:7:8:9mC\033[48:2::10:11:12mD\033[m'
printf '\033[38:2:0:13:14:15mE\033[m'
# black RGB is not the default colour
printf '\033[38;2;0;0;0mF\033[48;2;0;0;0mG\033[m'
# RGB then indexed, and indexed then RGB, clear each other
printf '\033[38;2;1;2;3;31mH\033[31;38;2;1;2;3mI\033[m'
# inverse and bold do not alter the stored value
printf '\033[1;7;38;2;200;100;50mJ\033[m'
# CMY and CMYK
printf '\033[38:3::0:100:100mK\033[38:4::0:0:0:50mL\033[m'
# erase with an RGB background fills with it (BCE)
printf '\033[48;2;9;9;9m\033[K\033[m'
# scroll RGB cells into the backlog
i=0
while [ $i -lt 30 ]; do
    printf '\033[38;2;%d;0;0m%d\033[m\r\n' "$i" "$i"
    i=$((i + 1))
done
