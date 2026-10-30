#!/bin/sh

printf '\033]4;1;#010101;2;#020202;3;#030303;20;#141414\007'
# reset two of them
printf '\033]104;1;20\007'
printf '\033]4;1;?;2;?;20;?\007'
# out of range is skipped, the rest still applies
printf '\033]104;999;2\033\\'
printf '\033]4;2;?\007'
# non-number stops: 3 stays set
printf '\033]104;x;3\007'
printf '\033]4;3;?\007'
# no parameter resets everything
printf '\033]104\007'
printf '\033]4;3;?\007'
printf '\033]4;5;#050505\007'
printf '\033]104;\007'
printf '\033]4;5;?\007'

## replies, with default_colors256 values:
# \e]4;1;rgb:c0c0/0000/0000\a \e]4;2;rgb:0000/c0c0/0000\a \e]4;20;rgb:c0c0/0000/0000\a
# \e]4;2;rgb:0000/c0c0/0000\a
# \e]4;3;rgb:c0c0/c0c0/0000\a
# \e]4;3;rgb:c0c0/c0c0/0000\a
# \e]4;5;rgb:8080/8080/8080\a
