#!/bin/sh
# base64: "clip\n" is Y2xpcAo=
printf '\033]52;p;\033\\\033]52;c;\033\\'
# an empty selector, as tmux sends, writes the clipboard
printf '\033]52;;Y2xpcAo=\033\\'
printf '\033]52;c;?\033\\'
printf '\033]52;p;?\033\\'
# and reads it
printf '\033]52;;?\033\\'
