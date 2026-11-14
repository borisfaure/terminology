#!/bin/sh
# kitty keyboard protocol: only implemented flags are stored and reported
printf '\033[>31u\033[?u'
printf '\033[=127u\033[?u'
printf '\033[=0u\033[=4;2u\033[?u'
printf '\033[<u'
