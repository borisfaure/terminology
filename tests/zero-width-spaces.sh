#!/bin/sh

# fill space with E
printf '\033#8'
#set color
printf '\033[46;31;3m'

# Zero width space
printf 'aa\xe2\x80\x8baa\n'

# Zero width space non-joiner
printf 'aa\xe2\x80\x8caa\n'

# Zero width space joiner
printf 'aa\xe2\x80\x8daa\n'
