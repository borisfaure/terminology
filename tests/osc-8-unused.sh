#!/bin/sh

# a link in use, which must survive what follows
printf '\033]8;;http://keep.example\033\\keep\033]8;;\033\\ '

# 80000 links never written to: replaced by the next one, or closed.
# More than the 65535 link slots, which they must not use up.
printf '\033]8;;http://a.example\033\\\033]8;;http://b.example\033\\\033]8;;\033\\%.0s' $(seq 40000)

# so that this one still gets a link
printf '\033]8;;http://last.example\033\\last\033]8;;\033\\'
