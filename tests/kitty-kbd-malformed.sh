#!/bin/sh
# kitty keyboard protocol: oversized parameters must not crash or push
printf '\033[>99999999999u\033[?u'
printf '\033[<99999999999u\033[?u'
printf '\033[=1;99999999999u\033[?u'
printf '\033[=1;0u\033[?u'
