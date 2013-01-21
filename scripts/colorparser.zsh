#!/bin/zsh
# Converts *color lines to edje color classes
# grep \*color ~/.Xresources | ./colorparser.zsh
while read line
do
    color_index=`echo $line | sed 's/\*color\([0-9][0-9]\?\):#[0-9a-Z]\+/\1/'`
    color=`echo $line | sed 's/\*color\([0-9][0-9]\?\):#\([0-9a-Z]\+\)/\2/'`
    color_r=$(echo "ibase=16;${color[1,2]:u}" | bc)
    color_g=$(echo "ibase=16;${color[3,4]:u}" | bc)
    color_b=$(echo "ibase=16;${color[5,6]:u}" | bc)
    echo "color_class { name: \"256color-$color_index\";  color: $color_r $color_g $color_b 255; }"
done
