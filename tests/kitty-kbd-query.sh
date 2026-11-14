#!/bin/sh
# kitty keyboard protocol: query, set modes and missing parameters
# default flags are 0
printf '\033[?u'
# CSI = flags u, mode defaults to 1 (assign)
printf '\033[=1u\033[?u'
# mode 3 clears, mode 2 ORs
printf '\033[=1;3u\033[?u'
printf '\033[=1;2u\033[?u'
# unknown mode is ignored
printf '\033[=0;4u\033[?u'
# missing flags means 0
printf '\033[=u\033[?u'
# the query ignores a parameter
printf '\033[=1u\033[?0u'
printf '\033[=0u'
# CSI u without parameter is still SCORC
printf '\033[5;7Hx\033[s\033[1;1H\033[uy'
