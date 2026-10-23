#!/bin/sh

# unknown, empty and near-miss requests are all invalid
printf '\033P$qz\033\\'
printf '\033P$q\033\\'
printf '\033P$q"px\033\\'
printf '\033P$q$|\033\\'
printf '\033P$q>4t\033\\'

# DECSCL still answers
printf '\033P$q"p\033\\'

# not a DECRQSS: no reply
printf '\033P$p\033\\'
