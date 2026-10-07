#!/bin/sh
# OSCs cancelled by CAN or SUB do nothing; the checksum covers the title
# and the replies.
long() {
   i=0
   while [ $i -lt "$1" ]; do printf 'QUFB'; i=$((i + 1)); done
}
printf '\033]2;kept\033\\'
# OSC cancelled by CAN
printf '\033]2;cancelled\030'
printf 'A'
# OSC cancelled by SUB
printf '\033]2;cancelled\032'
printf 'B'
# OSC introduced by the C1 OSC, cancelled by CAN
printf '\302\2352;cancelled\030'
printf 'C'
# CAN only cancels the OSC, the next sequence is parsed
printf '\033]2;cancelled\030\033[32mD'
# an OSC 52 set cancelled leaves the clipboard alone
printf '\033]52;c;Y2xpcAo=\033\\'
printf '\033]52;c;cHJpbQo=\030'
printf '\033]52;c;?\033\\'
# so does one longer than the OSC buffer
printf '\033]52;c;'; long 1250; printf '\032'
printf '\033]52;c;?\033\\'
# and a long OSC cancelled leaks nothing to the screen
printf '\033]2;'; long 1250; printf '\030'
printf 'E'
