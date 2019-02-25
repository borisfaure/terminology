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

# NE to SW {{{

setup_NE_to_SW()
{
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
# mouse down to start selection
printf '\033}td;395;148;1;4;0\0'
# mouse move
printf '\033}tm;175;160;4\0'
# mouse up
printf '\033}tu;175;160;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in t\n weak through the valley of darkn\0'
}

## sel "top-down" to NE
setup_NE_to_SW
# To
printf '\033}tm;450;78;0\0'
# double-click with shift
printf '\033}td;450;78;1;2;1\0'
printf '\033}tu;450;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}te and furious anger those who would attempt\nnd you will know My name is the Lord when I\n\n\ns man is beset on all sides by the iniquiti\n evil men. Blessed is he who, in the name of\n weak through the valley of darkness, for h\0'


## sel TO SE
setup_NE_to_SW
# To (with shift)
printf '\033}tm;510;187;0\0'
printf '\033}td;510;187;1;2;1\0'
printf '\033}tu;510;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\nhe finder of lost children. And I will strike down up\ne and furious anger those who would attempt to poison\0'


## sel TO to SW
setup_NE_to_SW
# To (with shift)
printf '\033}tm;51;187;0\0'
printf '\033}td;51;187;1;2;1\0'
printf '\033}tu;51;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\nther\047s keeper and the finder of lost children. And I\nwith great vengeance and furious anger those who wou\0'

## sel TO NW
setup_NE_to_SW
# To (with shift)
printf '\033}tm;51;78;0\0'
printf '\033}td;51;78;1;2;1\0'
printf '\033}tu;51;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tswith great vengeance and furious anger those who wou\ntroy My brothers. And you will know My name is the L\nupon thee.\n\nath of the righteous man is beset on all sides by th\nand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\0'

## sel TO middle
setup_NE_to_SW
# To (with shift)
printf '\033}tm;285;150;0\0'
printf '\033}td;285;150;1;2;1\0'
printf '\033}tu;285;150;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsBlessed is he who, in t\0'


# }}}
# SW to NE {{{

setup_SW_to_NE()
{
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
# mouse down to start selection
printf '\033}td;175;160;1;4;0\0'
# mouse move
printf '\033}tm;395;148;4\0'
# mouse up
printf '\033}tu;395;148;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in t\n weak through the valley of darkn\0'
}


## sel "top-down" to NE
setup_SW_to_NE
# To (with shift)
printf '\033}tm;510;78;0\0'
printf '\033}td;510;78;1;2;1\0'
printf '\033}tu;510;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tse and furious anger those who would attempt to poison\nnd you will know My name is the Lord when I lay My ve\n\n\ns man is beset on all sides by the iniquities of the\nevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\0'


## sel TO SE
setup_SW_to_NE
# To (with shift)
printf '\033}tm;510;187;0\0'
printf '\033}td;510;187;1;2;1\0'
printf '\033}tu;510;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\nhe finder of lost children. And I will strike down up\ne and furious anger those who would attempt to poison\0'


## sel TO to SW
setup_SW_to_NE
# To (with shift)
printf '\033}tm;51;187;0\0'
printf '\033}td;51;187;1;2;1\0'
printf '\033}tu;51;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\nther\047s keeper and the finder of lost children. And I\nwith great vengeance and furious anger those who wou\0'


## sel TO NW
setup_SW_to_NE
# To (with shift)
printf '\033}tm;51;78;0\0'
printf '\033}td;51;78;1;2;1\0'
printf '\033}tu;51;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tswith great vengeance and furious anger those who wou\ntroy My brothers. And you will know My name is the L\nupon thee.\n\nath of the righteous man is beset on all sides by th\nand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\0'

## sel TO middle
setup_SW_to_NE
# To (with shift)
printf '\033}tm;251;150;0\0'
printf '\033}td;251;150;1;2;1\0'
printf '\033}tu;251;150;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed\n weak through the\0'

# }}}
# NW to SE {{{

setup_NW_to_SE()
{
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
# mouse down to start selection
printf '\033}td;175;148;1;4;0\0'
# mouse move
printf '\033}tm;395;160;4\0'
# mouse up
printf '\033}tu;395;160;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in t\n weak through the valley of darkn\0'
}


## sel "top-down" to NE
setup_NW_to_SE
# To (with shift)
printf '\033}tm;510;78;0\0'
printf '\033}td;510;78;1;2;1\0'
printf '\033}tu;510;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tse and furious anger those who would attempt to poison\nnd you will know My name is the Lord when I lay My ve\n\n\ns man is beset on all sides by the iniquities of the\nevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\0'


## sel TO SE
setup_NW_to_SE
# To (with shift)
printf '\033}tm;510;187;0\0'
printf '\033}td;510;187;1;2;1\0'
printf '\033}tu;510;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\nhe finder of lost children. And I will strike down up\ne and furious anger those who would attempt to poison\0'


## sel TO to SW
setup_NW_to_SE
# To (with shift)
printf '\033}tm;51;187;0\0'
printf '\033}td;51;187;1;2;1\0'
printf '\033}tu;51;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\nther\047s keeper and the finder of lost children. And I\nwith great vengeance and furious anger those who wou\0'


## sel TO NW
setup_NW_to_SE
# To (with shift)
printf '\033}tm;51;78;0\0'
printf '\033}td;51;78;1;2;1\0'
printf '\033}tu;51;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tswith great vengeance and furious anger those who wou\ntroy My brothers. And you will know My name is the L\nupon thee.\n\nath of the righteous man is beset on all sides by th\nand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\0'

## sel TO middle
setup_NW_to_SE
# To (with shift)
printf '\033}tm;251;150;0\0'
printf '\033}td;251;150;1;2;1\0'
printf '\033}tu;251;150;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed\0'


# }}}
# SE to NW {{{

setup_SE_to_NW()
{
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
# mouse down to start selection
printf '\033}td;395;160;1;4;0\0'
# mouse move
printf '\033}tm;175;148;4\0'
# mouse up
printf '\033}tu;175;148;1;4;0\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in t\n weak through the valley of darkn\0'
}


## sel "top-down" to NE
setup_SE_to_NW
# To (with shift)
printf '\033}tm;510;78;0\0'
printf '\033}td;510;78;1;2;1\0'
printf '\033}tu;510;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tse and furious anger those who would attempt to poison\nnd you will know My name is the Lord when I lay My ve\n\n\ns man is beset on all sides by the iniquities of the\nevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\0'


## sel TO SE
setup_SE_to_NW
# To (with shift)
printf '\033}tm;510;187;0\0'
printf '\033}td;510;187;1;2;1\0'
printf '\033}tu;510;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsevil men. Blessed is he who, in the name of charity a\n weak through the valley of darkness, for he is truly\nhe finder of lost children. And I will strike down up\ne and furious anger those who would attempt to poison\0'


## sel TO to SW
setup_SE_to_NW
# To (with shift)
printf '\033}tm;51;187;0\0'
printf '\033}td;51;187;1;2;1\0'
printf '\033}tu;51;187;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\nther\047s keeper and the finder of lost children. And I\nwith great vengeance and furious anger those who wou\0'


## sel TO NW
setup_SE_to_NW
# To (with shift)
printf '\033}tm;51;78;0\0'
printf '\033}td;51;78;1;2;1\0'
printf '\033}tu;51;78;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tswith great vengeance and furious anger those who wou\ntroy My brothers. And you will know My name is the L\nupon thee.\n\nath of the righteous man is beset on all sides by th\nand the tyranny of evil men. Blessed is he who, in t\nwill, shepherds the weak through the valley of darkn\0'

## sel TO middle
setup_SE_to_NW
# To (with shift)
printf '\033}tm;251;150;0\0'
printf '\033}td;251;150;1;2;1\0'
printf '\033}tu;251;150;1;2;1\0'
printf '\033}tr\0'
# selection is
printf '\033}tsBlessed is he who, in t\nugh the valley of darkn\0'

# }}}

# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tr\0\033}tn\0'
