#!/bin/sh
# OSC 52 of 4095 codepoints with no ';' after the selector: looking for
# it used to read past the OSC buffer.
printf '\033]52;'
i=0
while [ $i -lt 1023 ]; do printf 'AAAA'; i=$((i + 1)); done
printf '\033\\'
printf '\033]52;'
i=0
while [ $i -lt 1023 ]; do printf 'AAAA'; i=$((i + 1)); done
printf '\007'
printf 'after'
