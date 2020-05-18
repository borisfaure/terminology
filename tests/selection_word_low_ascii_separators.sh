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
printf "This is a list of words with some low (in ascii) characters: \r\n"
printf "like efl-1.24.1 127.0.0.1 one+plus+one aaa\045aaa \r\n"
printf "and now some separators:\r\n"
printf "times*times comma,separates \042aaa\042 \044aaa\044 #aaa# !aaa!"
# force render
printf '\033}tr\0'

## efl-1.24.1
# double click
printf '\033}td;40;40;1;0;0\0'
printf '\033}tu;40;40;1;0;0\0'
printf '\033}td;40;40;1;0;1\0'
printf '\033}tu;40;40;1;0;1\0'
# force render
printf '\033}tr\0'
# assert selection is
printf '\033}tsefl-1.24.1\0'
# remove selection
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## 127.0.0.1
printf '\033}td;150;40;1;0;0\0'
printf '\033}tu;150;40;1;0;0\0'
printf '\033}td;150;40;1;0;1\0'
printf '\033}tu;150;40;1;0;1\0'
printf '\033}tr\0'
printf '\033}ts127.0.0.1\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## one+plus+one
printf '\033}td;205;40;1;0;0\0'
printf '\033}tu;205;40;1;0;0\0'
printf '\033}td;205;40;1;0;1\0'
printf '\033}tu;205;40;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsone+plus+one\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## aaa%aaa
printf '\033}td;275;40;1;0;0\0'
printf '\033}tu;275;40;1;0;0\0'
printf '\033}td;275;40;1;0;1\0'
printf '\033}tu;275;40;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsaaa\045aaa\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## times*times
printf '\033}td;50;65;1;0;0\0'
printf '\033}tu;50;65;1;0;0\0'
printf '\033}td;50;65;1;0;1\0'
printf '\033}tu;50;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tstimes\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## comma,separates
printf '\033}td;110;65;1;0;0\0'
printf '\033}tu;110;65;1;0;0\0'
printf '\033}td;110;65;1;0;1\0'
printf '\033}tu;110;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tscomma\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## "aaa"
printf '\033}td;220;65;1;0;0\0'
printf '\033}tu;220;65;1;0;0\0'
printf '\033}td;220;65;1;0;1\0'
printf '\033}tu;220;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsaaa\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## $aaa$
printf '\033}td;250;65;1;0;0\0'
printf '\033}tu;250;65;1;0;0\0'
printf '\033}td;250;65;1;0;1\0'
printf '\033}tu;250;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsaaa\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## #aaa#
printf '\033}td;290;65;1;0;0\0'
printf '\033}tu;290;65;1;0;0\0'
printf '\033}td;290;65;1;0;1\0'
printf '\033}tu;290;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsaaa\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'

## !aaa!
printf '\033}td;330;65;1;0;0\0'
printf '\033}tu;330;65;1;0;0\0'
printf '\033}td;330;65;1;0;1\0'
printf '\033}tu;330;65;1;0;1\0'
printf '\033}tr\0'
printf '\033}tsaaa\0'
printf '\033}td;0;0;1;0;0\0\033}tu;0;0;1;0;0\0'
printf '\033}tc;0;0\0\033}tc;1;0\0'
