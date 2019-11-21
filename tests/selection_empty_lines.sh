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
printf "%s\r\n\r\n%s" "$TEXT" "$TEXT"


# mouse down to start selection
printf '\033}td;16;85;1;0;0\0'

# mouse move
printf '\033}tm;244;150;0\0'

# mouse up
printf '\033}tu;244;150;1;0;0\0'

# force render
printf '\033}tr\0'

# selection is
printf '\033}tsdestroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee.\n\nThe path of the righteous man is beset on all sides by the iniquities of the selfish and the tyranny of evil men. B\0'

# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'

# force render
printf '\033}tr\0'

# no more selection
printf '\033}tn\0'
