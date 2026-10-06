#!/bin/sh
# OSCs too long for the 4096 codepoint buffer are discarded whole: none
# of their content may reach the screen.
long() {
   i=0
   while [ $i -lt "$1" ]; do printf 'QUFB'; i=$((i + 1)); done
}
# ended by ESC \
printf '\033]2;'; long 1250; printf '\033\\'
printf 'one\r\n'
# ended by BEL
printf '\033]2;'; long 1250; printf '\007'
printf 'two\r\n'
# an ESC not followed by \ does not end it
printf '\033]2;'; long 1250; printf '\033x'; long 10; printf '\033\\'
printf 'three\r\n'
# 4096 codepoints leave no room for the NUL, the terminator follows
printf '\033]2;'; long 1023; printf 'ab\033\\'
printf 'four\r\n'
# far longer than the buffer
printf '\033]2;'; long 25000; printf '\033\\'
printf 'five\r\n'
# none of them set the title, which the checksum covers
