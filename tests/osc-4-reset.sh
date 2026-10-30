#!/bin/sh

# RIS and DECSTR reset the palette in xterm: ReallyReset runs before the
# "if (full)" branch, and both VTReset paths reach it. Terminology's
# DECSTR already restores the OSC 10/11/12 colours the same way.

printf '\033]4;1;#010101;100;#646464\007'
printf '\033[!p'
printf '\033]4;1;?;100;?\007'
printf '\033]4;1;#010101\007'
printf '\033c'
printf '\033]4;1;?\007'

## replies: both queries return the defaults
# \e]4;1;rgb:c0c0/0000/0000\a \e]4;100;rgb:8787/8787/0000\a
# \e]4;1;rgb:c0c0/0000/0000\a
