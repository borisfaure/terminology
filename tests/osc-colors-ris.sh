#!/bin/sh

# RIS and DECSTR restore the default background and cursor colours
printf '\033]11;#00ff00\007\033]12;#0000ff\007'
printf '\033c'
printf '\033]11;?\007\033]12;?\007'
printf '\033]11;#00ff00\007'
printf '\033[!p'
printf '\033]11;?\007'
