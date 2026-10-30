#!/bin/sh

# OSC 4 sets, every spec form, then read back with queries

printf '\033]4;1;#ff0000\007'
printf '\033]4;2;#00ff00\033\\'
printf '\033]4;3;rgb:12/34/56\007'
printf '\033]4;4;rgb:f/0/8\007'
printf '\033]4;5;rgb:1234/5678/9abc\007'
printf '\033]4;6;rgbi:1/0.5/0\007'
printf '\033]4;7;#123456789\007'
printf '\033]4;8;#fff\007'
printf '\033]4;1;?;2;?;3;?;4;?;5;?;6;?;7;?;8;?\007'
# set and query in one sequence
printf '\033]4;200;#010203;200;?\007'
# bad spec stops: 10 is set, 11 is not, 12 is not
printf '\033]4;10;#0a0a0a;11;nope;12;#0c0c0c\007'
printf '\033]4;10;?;11;?;12;?\007'
# out of range index stops the rest
printf '\033]4;13;#0d0d0d;300;#ffffff;14;#0e0e0e\007'
printf '\033]4;13;?;14;?\007'
# a set colour is used by the truecolour approximation
printf '\033]4;232;#3b224c\007'
printf '\033[48;2;59;34;76mX\033[m'

## replies:
# \e]4;1;rgb:ffff/0000/0000\a
# \e]4;2;rgb:0000/ffff/0000\a
# \e]4;3;rgb:1212/3434/5656\a
# \e]4;4;rgb:ffff/0000/8888\a
# \e]4;5;rgb:1212/5656/9a9a\a
# \e]4;6;rgb:ffff/8080/0000\a
# \e]4;7;rgb:1212/4545/7878\a
# \e]4;8;rgb:f0f0/f0f0/f0f0\a
# \e]4;200;rgb:0101/0202/0303\a
# \e]4;10;rgb:0a0a/0a0a/0a0a\a  (11 and 12 answer the defaults: the bad
# \e]4;11;rgb:ffff/ffff/8080\a  spec stopped the parsing before they
# \e]4;12;rgb:8080/8080/ffff\a  could be set)
# \e]4;13;rgb:0d0d/0d0d/0d0d\a \e]4;14;rgb:8080/ffff/ffff\a
# (the out-of-range 300 stopped the sets, so 14 is still its default;
# the queries start a new sequence, and 14 is answered)
