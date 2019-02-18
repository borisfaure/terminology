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
printf "The path of the righteous man is beset on all sides by the iniquities of the selfish and the tyranny of evil men. Blessed is he who, in the name of charity and good will, shepherds the weak through the valley of darkness, for he is truly his brother's keeper and the finder of lost children. And I will strike down upon thee with great vengeance and furious anger those who would attempt to poison and destroy My brothers. And you will know My name is the Lord when I lay My vengeance upon thee.\r\n"
printf "<foo@bar.com> https://terminolo.gy/ \r\n"
printf '(127.0.0.1) [10.10.0.1:443] {localhost}'
# force render
printf '\033}tr\0'

## simple word "good"
printf '\033}td;2;50;1;0;0\0'
printf '\033}tu;2;50;1;0;0\0'
printf '\033}td;2;50;1;0;1\0'
printf '\033}tu;2;50;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsgood\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## word ending with . "men"
printf '\033}td;212;43;1;0;0\0'
printf '\033}tu;212;43;1;0;0\0'
printf '\033}td;212;43;1;0;1\0'
printf '\033}tu;212;43;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsmen\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## ' ' -> no selection
printf '\033}td;71;50;1;0;0\0'
printf '\033}tu;71;50;1;0;0\0'
printf '\033}td;71;50;1;0;1\0'
printf '\033}tu;71;50;1;0;1\0'
# force render
printf '\033}tr\0'
# no selection
printf '\033}tn\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## word ending with , "will"
printf '\033}td;50;50;1;0;0\0'
printf '\033}tu;50;50;1;0;0\0'
printf '\033}td;50;50;1;0;1\0'
printf '\033}tu;50;50;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tswill\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## ','
printf '\033}td;69;50;1;0;0\0'
printf '\033}tu;69;50;1;0;0\0'
printf '\033}td;69;50;1;0;1\0'
printf '\033}tu;69;50;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts,\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# word on 2 lines "venge-ance"
printf '\033}td;680;99;1;0;0\0'
printf '\033}tu;680;99;1;0;0\0'
printf '\033}td;680;99;1;0;1\0'
printf '\033}tu;680;99;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsvengeance\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'
#from end for word
printf '\033}td;25;110;1;0;0\0'
printf '\033}tu;25;110;1;0;0\0'
printf '\033}td;25;110;1;0;1\0'
printf '\033}tu;25;110;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsvengeance\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# email
printf '\033}td;46;128;1;0;0\0'
printf '\033}tu;46;128;1;0;0\0'
printf '\033}td;46;128;1;0;1\0'
printf '\033}tu;46;128;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsfoo@bar.com\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'


# url
printf '\033}td;99;126;1;0;0\0'
printf '\033}tu;99;126;1;0;0\0'
printf '\033}td;99;126;1;0;1\0'
printf '\033}tu;99;126;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tshttps://terminolo.gy/\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'
# same
printf '\033}td;239;126;1;0;0\0'
printf '\033}tu;239;126;1;0;0\0'
printf '\033}td;239;126;1;0;1\0'
printf '\033}tu;239;126;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tshttps://terminolo.gy/\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# IP
printf '\033}td;36;142;1;0;0\0'
printf '\033}tu;36;142;1;0;0\0'
printf '\033}td;36;142;1;0;1\0'
printf '\033}tu;36;142;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts127.0.0.1\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# IP:PORT
printf '\033}td;136;142;1;0;0\0'
printf '\033}tu;136;142;1;0;0\0'
printf '\033}td;136;142;1;0;1\0'
printf '\033}tu;136;142;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}ts10.10.0.1:443\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

# word in curly brackets
printf '\033}td;236;142;1;0;0\0'
printf '\033}tu;236;142;1;0;0\0'
printf '\033}td;236;142;1;0;1\0'
printf '\033}tu;236;142;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tslocalhost\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'
