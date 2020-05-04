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
printf "%s\r\n%s\r\n%s\r\n" "1>$TEXT" "2>$TEXT" "3>$TEXT"
printf "%s\r\n%s\r\n%s\r\n" "4>$TEXT" "5>$TEXT" "6>$TEXT"
# force render
printf '\033}tr\0'

# mouse wheel up
for _ in $(seq 5); do
   printf '\033}tw;0;0;1;1;0\0'
done

# force render
printf '\033}tr\0'

## box selection
# mouse down to start selection
printf '\033}td;35;118;1;4;0\0'
# mouse move
printf '\033}tm;125;135;4\0'
# mouse up
printf '\033}tu;125;135;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsce upon thee.\047e path of the 0'
