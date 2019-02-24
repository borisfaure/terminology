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


## sel "top-down" to down
# mouse down to start selection
printf '\033}td;395;148;1;0;0\0'
# mouse move
printf '\033}tm;25;160;0\0'
# mouse up
printf '\033}tu;25;160;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;210;178;0\0'
# mouse double-click with shift
printf '\033}td;210;178;1;2;1\0'
printf '\033}tu;210;178;1;2;1\0'
# selection is
printf '\033}tsthe name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother\047s keeper and the finder\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


## sel "top-down" to up
# mouse down to start selection
printf '\033}td;395;148;1;0;0\0'
# mouse move
printf '\033}tm;25;160;0\0'
# mouse up
printf '\033}tu;25;160;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;500;128;0\0'
# mouse double-click with shift
printf '\033}td;500;128;1;2;1\0'
printf '\033}tu;500;128;1;2;1\0'
# selection is
printf '\033}tsof the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'

## sel "top_down" to within selection
# mouse down to start selection
printf '\033}td;395;148;1;0;0\0'
# mouse move
printf '\033}tm;25;160;0\0'
# mouse up
printf '\033}tu;25;160;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;500;150;0\0'
# mouse double-click with shift
printf '\033}td;500;150;1;2;1\0'
printf '\033}tu;500;150;1;2;1\0'
# selection is
printf '\033}tsthe name of charity\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


## sel "down-top" to down
# mouse down to start selection
printf '\033}td;25;160;1;0;0\0'
# mouse move
printf '\033}tm;395;148;0\0'
# mouse up
printf '\033}tu;395;148;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;210;178;0\0'
# mouse double-click with shift
printf '\033}td;210;178;1;2;1\0'
printf '\033}tu;210;178;1;2;1\0'
# selection is
printf '\033}tsthe name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother\047s keeper and the finder\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


## sel "down-top" to up
# mouse down to start selection
printf '\033}td;25;160;1;0;0\0'
# mouse move
printf '\033}tm;395;148;0\0'
# mouse up
printf '\033}tu;395;148;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;500;128;0\0'
# mouse double-click with shift
printf '\033}td;500;128;1;2;1\0'
printf '\033}tu;500;128;1;2;1\0'
# selection is
printf '\033}tsof the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'


## sel "down-top" to within
# mouse down to start selection
printf '\033}td;25;160;1;0;0\0'
# mouse move
printf '\033}tm;395;148;0\0'
# mouse up
printf '\033}tu;395;148;1;0;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsthe name of charity and good\0'
# To
# mouse move
printf '\033}tm;500;150;0\0'
# mouse double-click with shift
printf '\033}td;500;150;1;2;1\0'
printf '\033}tu;500;150;1;2;1\0'
# selection is
printf '\033}tscharity and good\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
