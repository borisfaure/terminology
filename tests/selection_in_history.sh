#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 2,0
printf '\033[2H'

# set text
TEXT="The path of the righteous man is beset on all sides by the iniquities of the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother's keeper and the finder of lost children. And I will strike down upon thee with great vengeance and furious anger those who would attempt to poison and destroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee."

# display text
printf "%s\r\n%s\r\n%s" "$TEXT" "$TEXT" "$TEXT"
# force render
printf '\033}tr\0'

## full paragraph
printf '\033}td;2;50;1;0;0\0'
printf '\033}tu;2;50;1;0;0\0'
printf '\033}td;2;50;1;0;1\0'
printf '\033}tu;2;50;1;0;1\0'
printf '\033}td;2;50;1;0;2\0'
printf '\033}tu;2;50;1;0;2\0'
# force render
printf '\033}tr\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


# mouse down to start selection
printf '\033}td;395;148;1;0;0\0'
# mouse move
printf '\033}tm;525;148;0\0'
# mouse up
printf '\033}tu;525;148;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity\0'

# scroll 30 lines
for L in $(seq 30); do
   printf '\r\n%s' "$L"
done

# force render
printf '\033}tr\0'

## simple selection
printf '\033}td;0;14;1;0;0\0'
printf '\033}tm;8;14\0'
printf '\033}tm;7;14\0'
printf '\033}tu;7;14;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is '7'
printf '\033}ts7\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'

# one wheel up
printf '\033}tw;0;0;1;1;0\0'

## simple selection
printf '\033}td;0;14;1;0;0\0'
printf '\033}tm;8;14\0'
printf '\033}tm;7;14\0'
printf '\033}tu;7;14;1;0;0\0'
# force render
printf '\033}tr\0'

# selection is '3'
printf '\033}ts3\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'



# mouse wheel up
for _ in $(seq 5); do
   printf '\033}tw;0;0;1;1;0\0'
done

# force render
printf '\033}tr\0'


## normal selection
# mouse down to start selection
printf '\033}td;35;118;1;0;0\0'
# mouse move
printf '\033}tm;125;135;0\0'
# mouse up
printf '\033}tu;125;135;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsother\047s keeper and the finder of lost children. And I will strike down upon thee with great ve\0'

# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'

## double click: word selection
printf '\033}td;230;134;1;0;0\0'
printf '\033}tu;230;134;1;0;0\0'
printf '\033}td;230;134;1;0;1\0'
printf '\033}tu;230;134;1;0;1\0'

# force render
printf '\033}tr\0'

# selection is
printf '\033}tsfurious\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


## triple click: full paragraph selection
printf '\033}td;236;132;1;0;0\0'
printf '\033}tu;236;132;1;0;0\0'
printf '\033}td;236;132;1;0;1\0'
printf '\033}tu;236;132;1;0;1\0'
printf '\033}td;236;132;1;0;2\0'
printf '\033}tu;236;132;1;0;2\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts%s\0' "$TEXT"
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
