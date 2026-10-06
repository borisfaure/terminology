#!/bin/sh
# OSC content of exactly 4095 codepoints: "52;ppc;" + 4088 base64 chars.
# Setting used to write a NUL two cells past the OSC buffer.
printf '\033]52;ppc;'
i=0
while [ $i -lt 1022 ]; do printf 'QUFB'; i=$((i + 1)); done
printf '\033\\'
# the same with a BEL terminator
printf '\033]52;ppc;'
i=0
while [ $i -lt 1022 ]; do printf 'QUFB'; i=$((i + 1)); done
printf '\007'
printf '\033]52;p;?\033\\'
