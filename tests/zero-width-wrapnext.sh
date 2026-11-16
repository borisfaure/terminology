#!/bin/sh

# zero-width characters take no cell: one written after the last column
# must leave the pending wrap alone, and one written in insert mode must
# not push the row aside

printf '\033[2J\033[H'

# a combining grave accent after the last column, then a new line: the
# next text lands on the row just below
printf '\033[1;71H0123456789\314\200\r\nbelow 1'

# same with a zero width space and a variation selector
printf '\033[3;71H0123456789\342\200\213\r\nbelow 3'
printf '\033[5;71H0123456789\357\270\200\r\nbelow 5'

# a combining long stroke overlay: the pending wrap still happens on the
# next printable character, which gets struck through
printf '\033[7;71H0123456789\314\266X'

# at a right margin
printf '\033[?69h\033[10;40s'
printf '\033[10;31H0123456789\314\200\r\nbelow 10'
printf '\033[?69l'

# in insert mode
printf '\033[13;1Hcombining'
printf '\033[4h\033[13;1H\314\200\342\200\213\357\270\200\033[4l'
printf '\033[13;20H'
