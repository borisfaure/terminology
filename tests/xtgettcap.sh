#!/bin/sh
# XTGETTCAP: DCS + q Pt ST, answered from the terminology terminfo
# Ms (opentui's probe), lowercase hex as opentui sends it
printf '\033P+q4d73\033\\'
# the same, uppercase
printf '\033P+q4D73\033\\'
# TN follows the configured TERM, name is its terminfo spelling
printf '\033P+q544e\033\\'
printf '\033P+q6e616d65\033\\'
# Co and colors
printf '\033P+q436f;636f6c6f7273\033\\'
# a boolean
printf '\033P+q616d\033\\'
# a key string
printf '\033P+q6b63757531\033\\'
# unknown, then known: one reply each, processing goes on
printf '\033P+q787878;4373\033\\'
# RGB is not claimed
printf '\033P+q524742\033\\'
# invalid hex: odd length, non-hex digit, encoded NUL
printf '\033P+q4d7\033\\'
printf '\033P+q4g73\033\\'
printf '\033P+q0000\033\\'
# empty list, empty item
printf '\033P+q\033\\'
printf '\033P+q4d73;\033\\'
# a C1 ST terminator
printf '\033P+q616d\302\234'
# nothing printed on screen
printf 'done'
