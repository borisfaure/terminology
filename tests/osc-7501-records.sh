#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# 257 records: the cap is 256, so the least recently updated one goes.
# Refreshing r0 before the 257th makes r1 the one evicted.
i=0
while [ $i -lt 256 ]; do
    printf '\033]7501;id=r%d:state=idle\033\\' "$i"
    i=$((i+1))
done
printf '\033]7501;id=r0:state=working\033\\'
printf '\033]7501;id=r256:state=idle\033\\'

# a parent with children, and a sibling sharing the parent's prefix
printf '\033]7501;id=a:state=working\033\\'
printf '\033]7501;id=a/b:state=working\033\\'
printf '\033]7501;id=a/b/c:state=done\033\\'
printf '\033]7501;id=ab:state=error\033\\'
# clear on the parent takes the children, the sibling stays
printf '\033]7501;id=a:state=clear\033\\'
# clear with no id takes everything
printf '\033]7501;state=clear\033\\'

# RIS removes every record
printf '\033]7501;id=m1:state=working\033\\'
printf '\033]7501;id=m2:state=done\033\\'
printf '\033c'

# records survive the alternate screen and DECSTR
printf '\033[?1049h'
printf '\033]7501;id=alt:state=working\033\\'
printf '\033[!p'
printf '\033[?1049l'
printf '\033]7501;id=s1:state=done\033\\'
printf '\033]7501;id=s2:state=error\007'
