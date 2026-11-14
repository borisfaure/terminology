#!/bin/sh
# kitty keyboard protocol: push, pop, eviction and over-pop
# push without parameter pushes 0
printf '\033[>1u\033[>u\033[?u'
# pop without parameter pops 1
printf '\033[<u\033[?u'
# pop more than the depth resets to 0
printf '\033[<10u\033[?u'
# CSI < 0 u pops nothing
printf '\033[>1u\033[<0u\033[?u'
printf '\033[<u'
# 9 pushes into a stack of 8 evict the oldest:
# base 0 then 1,0,1,0,1,0,1 fills the 8 slots,
# the 8th push (0) evicts the base, the 9th (1) evicts the leading 1,
# leaving 0,1,0,1,0,1,0,1
printf '\033[>1u\033[>0u\033[>1u\033[>0u\033[>1u'
printf '\033[>0u\033[>1u\033[>0u\033[>1u'
# pop 7: the remaining entry is the second push, 0
printf '\033[<7u\033[?u'
# one more pop empties the stack
printf '\033[<u\033[?u'
# set on an empty stack, then push and pop return to it
printf '\033[=1u\033[>0u\033[?u\033[<u\033[?u'
printf '\033[<u\033[?u'
