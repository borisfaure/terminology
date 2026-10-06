#!/bin/sh
# base64: "clip\n" is Y2xpcAo=
# not a multiple of 4 characters: the decoder rejects it
printf '\033]52;c;Y2xpcAo=\033\\'
printf '\033]52;c;!!!\033\\'
printf '\033]52;c;?\033\\'
# a multiple of 4 characters, none of them base64
printf '\033]52;c;Y2xpcAo=\033\\'
printf '\033]52;c;!!!!\033\\'
printf '\033]52;c;?\033\\'
