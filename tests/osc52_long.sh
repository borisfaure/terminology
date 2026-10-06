#!/bin/sh
# OSC 52 sets longer than the 4096 codepoint OSC buffer are kept whole.
# QUFB is the base64 of AAA, so the replies repeat the payloads.
long() {
   i=0
   while [ $i -lt "$1" ]; do printf "$2"; i=$((i + 1)); done
}
printf '\033]52;p;\033\\\033]52;c;\033\\'
# ended by ESC \, to both selections
printf '\033]52;pc;'; long 2500 'QUFB'; printf '\033\\'
printf '\033]52;p;?\033\\\033]52;c;?\033\\'
# ended by BEL, the empty selector meaning the clipboard
printf '\033]52;;'; long 1500 'QkJC'; printf '\007'
printf '\033]52;c;?\033\\\033]52;p;?\033\\'
# an unsupported selector: discarded, and nothing changes
printf '\033]52;q;'; long 1500 'Q0ND'; printf '\033\\'
printf '\033]52;c;?\033\\'
# nothing of it reaches the screen
printf 'after'
