#!/bin/sh

# fill space with E
printf '\033#8'

# iTerm2 notification: the whole payload is the summary
printf '\033]9;Build finished\007'
# semicolons inside the text are kept, no title/body split
printf '\033]9;done; 3 warnings\033\\'
# what opentui sends to iTerm2
printf '\033]9;OpenCode: Session done\033\\'
# ConEmu sequences are never notifications
printf '\033]9;5\007'
printf '\033]9;1;100\007'
printf '\033]9;9;/tmp\033\\'
printf '\033]9;42\033\\'
# malformed progress: not a notification either
printf '\033]9;4\007'
printf '\033]9;4;\007'
printf '\033]9;4;5\007'
printf '\033]9;4;tests failed\007'
# text that only looks numeric is text
printf '\033]9;4x\007'
printf '\033]9;12 done\007'
# empty: nothing
printf '\033]9;\007'
