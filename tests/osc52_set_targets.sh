#!/bin/sh
# base64: "clip\n" is Y2xpcAo=, "prim\n" is cHJpbQo=
printf '\033]52;p;\033\\\033]52;c;\033\\'
# both targets
printf '\033]52;pc;Y2xpcAo=\033\\'
printf '\033]52;p;?\033\\\033]52;c;?\033\\'
# repeated letters, s as primary
printf '\033]52;ssp;cHJpbQo=\033\\'
printf '\033]52;p;?\033\\\033]52;c;?\033\\'
# unsupported letters do not land in primary any more
printf '\033]52;p;\033\\'
printf '\033]52;q;Y2xpcAo=\033\\'
printf '\033]52;p;?\033\\'
# clearing both at once
printf '\033]52;pc;\033\\'
printf '\033]52;p;?\033\\\033]52;c;?\033\\'
