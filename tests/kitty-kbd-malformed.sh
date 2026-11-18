#!/bin/sh
# kitty keyboard protocol: oversized parameters are ignored, not clamped
# start from 1, so that each one would change the answer if applied
printf '\033[>1u'
printf '\033[>99999999999u\033[?u'
printf '\033[<99999999999u\033[?u'
printf '\033[=0;99999999999u\033[?u'
printf '\033[=0;0u\033[?u'
printf '\033[<u'
