#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 0,0
printf '\033[H'

# set text
TEXT="The path of the righteous man is beset on all sides by the iniquities of the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother's keeper and the finder of lost children. And I will strike down upon thee with great vengeance and furious anger those who would attempt to poison and destroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee."

# display text
printf "%s\r\n%s\r\n%s\r\n" "1>$TEXT" "2>$TEXT" "3>$TEXT"
printf "%s\r\n%s\r\n%s\r\n" "4>$TEXT" "5>$TEXT" "6>$TEXT"

# mouse wheel to scroll up
for _ in $(seq 5); do
   printf '\033}tw;0;0;1;1;0\0'
done

# mouse down to start selection
printf '\033}td;70;315;1;0;0\0'
# mouse move
printf '\033}tm;100;315;0\0'
# make scroll down
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
printf '\033}tm;100;359;0\0'
# mouse up
printf '\033}tu;100;358;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is active
printf '\033}tn!\0'
