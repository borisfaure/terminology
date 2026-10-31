#!/bin/sh

# fill space with E
printf '\033#8'

# o=always: shown (the default)
printf '\033]99;o=always;always\033\\'
# an unknown occasion value: treated as unset, shown
printf '\033]99;o=sometimes;sometimes\033\\'
# o=unfocused: the tytest termio stubs report "visible but not focused",
# so it is honoured
printf '\033]99;o=unfocused;unfocused\033\\'
# o=invisible: the term is in view, so it is not honoured
printf '\033]99;o=invisible;invisible\033\\'
