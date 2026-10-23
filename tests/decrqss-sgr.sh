#!/bin/sh

# default
printf '\033P$qm\033\\'

# every attribute
printf '\033[1;2;3;4;5;6;7;8;9;20;51;52;53m'
printf '\033P$qm\033\\'
printf '\033[m'

# the colour forms
printf '\033[31;42m\033P$qm\033\\\033[m'
printf '\033[97;107m\033P$qm\033\\\033[m'
printf '\033[99;109m\033P$qm\033\\\033[m'
printf '\033[38;5;1;48;5;200m\033P$qm\033\\\033[m'
printf '\033[38;1;48;1m\033P$qm\033\\\033[m'

# RGB colours are reported as 38:2::r:g:b / 48:2::r:g:b, black included
printf '\033[38;2;59;34;76;48;2;59;34;76m\033P$qm\033\\\033[m'
printf '\033[38;2;0;0;0;48;2;0;0;0m\033P$qm\033\\\033[m'

# resets
printf '\033[1;31m\033[22;39m\033P$qm\033\\'
printf '\033[7m\033c\033P$qm\033\\'
