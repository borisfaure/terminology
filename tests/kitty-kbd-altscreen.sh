#!/bin/sh
# kitty keyboard protocol: one stack per screen, kept across switches
printf '\033[>1u'
# 1049: the alternate screen starts with its own empty stack
printf '\033[?1049h\033[?u'
printf '\033[>1u\033[?u'
# back on main: still 1
printf '\033[?1049l\033[?u'
# the alternate stack survived the switch
printf '\033[?1049h\033[?u\033[<u\033[?1049l'
# 47 and 1047 select the same stacks
printf '\033[?47h\033[?u\033[?47l'
printf '\033[?1047h\033[>1u\033[?1047l\033[?u'
printf '\033[?1047h\033[?u\033[<u\033[?1047l'
printf '\033[<u\033[?u'
