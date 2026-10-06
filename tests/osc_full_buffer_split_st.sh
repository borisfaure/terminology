#!/bin/sh
# OSC 2 whose content fills the OSC buffer (4095 codepoints) but for the
# terminating NUL: read byte by byte, ESC used to land in the last slot,
# be taken for an overflow, and leave '\' printed.
printf '\033]2;'
i=0
while [ $i -lt 1023 ]; do printf 'abcd'; i=$((i + 1)); done
printf 'z\033\\'
printf 'after'
