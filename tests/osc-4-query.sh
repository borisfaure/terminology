#!/bin/sh

# OSC 4 queries: one reply per '?', in request order, echoing the
# request's terminator (BEL in, BEL out; ST in, ST out)

# BEL in, BEL out
printf '\033]4;1;?\007'
# ST in, ST out
printf '\033]4;9;?\033\\'
# cube and grey ramp ends
printf '\033]4;16;?\007'
printf '\033]4;231;?\007'
printf '\033]4;255;?\007'
# several queries in one sequence: one reply each, in order
printf '\033]4;0;?;15;?;196;?\007'
# opentui's burst: sixteen queries back to back
i=0
while [ $i -lt 16 ]; do
    printf '\033]4;%d;?\007' "$i"
    i=$((i + 1))
done
# out of range stops: only 3 is answered
printf '\033]4;3;?;256;?;4;?\007'
# non-numeric index stops: nothing
printf '\033]4;x;?\007'
# missing spec: nothing
printf '\033]4;5\007'
# bare OSC 4 and OSC 4; : nothing
printf '\033]4\007'
printf '\033]4;\007'

## replies:
# \e]4;1;rgb:c0c0/0000/0000\a
# \e]4;9;rgb:ffff/8080/8080\e\
# \e]4;16;rgb:0000/0000/0000\a
# \e]4;231;rgb:ffff/ffff/ffff\a
# \e]4;255;rgb:eeee/eeee/eeee\a
# \e]4;0;rgb:0000/0000/0000\a \e]4;15;rgb:ffff/ffff/ffff\a \e]4;196;rgb:ffff/0000/0000\a
# sixteen replies, for 0 to 15
# \e]4;3;rgb:c0c0/0000/0000\a
