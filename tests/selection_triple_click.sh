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

#move to 18,0
printf '\033[18H'
printf "foo bar qux\r\nfoo bar qux"

printf '\033[21H'
printf "foo bar qux"
printf '\033[21;50H'
printf "foo bar qux"

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
# selection is
printf '\033}ts%s\0' "$TEXT"
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

