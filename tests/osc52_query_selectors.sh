#!/bin/sh
# base64: "clip\n" is Y2xpcAo=, "prim\n" is cHJpbQo=
# start empty
printf '\033]52;p;\033\\\033]52;c;\033\\'
# nothing set: one empty reply, with the first supported letter
printf '\033]52;pc;?\033\\'
# only the clipboard set: pc must find it
printf '\033]52;c;Y2xpcAo=\033\\'
printf '\033]52;pc;?\033\\'
printf '\033]52;cp;?\033\\'
# both set: the first listed wins, one reply only
printf '\033]52;p;cHJpbQo=\033\\'
printf '\033]52;pc;?\033\\'
printf '\033]52;cp;?\033\\'
# s reads primary and is echoed as s
printf '\033]52;s;?\033\\'
# unsupported letters are skipped
printf '\033]52;q0c;?\033\\'
# only unsupported letters: no reply at all
printf '\033]52;q7;?\033\\'
# BEL-terminated request still gets one ST-terminated reply
printf '\033]52;c;?\007'
