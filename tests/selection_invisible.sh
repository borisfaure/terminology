#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

#move to 4,0
printf '\033[4H'

# set text
printf "The path of the righteous man is beset on all sides by the iniquities of the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother's keeper and the finder of lost children. And I will strike down upon thee with great vengeance and furious anger those who would attempt to poison and destroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee.\n"

#move to 3,80
printf '\033[3;80H#'
#move to 10,80
printf '\033[10;80H#'

## Top in "invisible" to down
# mouse down to start selection
printf '\033}td;16;35;1;0;0\0'
# mouse move
printf '\033}tm;44;50;0\0'
# mouse up
printf '\033}tu;44;50;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts                                                                             #\nThe pat\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0'
printf '\033}tn\0'

## Down to top in "invisible"
# mouse down to start selection
printf '\033}td;44;50;1;0;0\0'
# mouse move
printf '\033}tm;16;35;0\0'
# mouse up
printf '\033}tu;16;35;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts                                                                             #\nThe pat\0'

# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0'
printf '\033}tn\0'


## Top to down in "invisible"
# mouse down to start selection
printf '\033}td;80;140;1;0;0\0'
# mouse move
printf '\033}tm;44;160;0\0'
# mouse up
printf '\033}tu;44;160;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tshee.                                                                #\n\n\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0'
printf '\033}tn\0'

## Down in "invisible" to top
# mouse down to start selection
printf '\033}td;44;160;1;0;0\0'
# mouse move
printf '\033}tm;16;135;0\0'
# mouse up
printf '\033}tu;16;135;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsdestroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee.                                                                #\n\n\0'

# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0'
printf '\033}tn\0'
