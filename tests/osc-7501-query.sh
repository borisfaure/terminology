#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# feature detection: a lone question mark gets the fixed reply,
# whatever the terminator
printf '\033]7501;?\033\\'
printf '\033]7501;?\007'
# whitespace around the question mark changes nothing
printf '\033]7501; ? \033\\'
printf '\033]7501;\t?\t\033\\'
# a question mark inside a report is a malformed pair with no state:
# ignored, no reply
printf '\033]7501;?x\033\\'
printf '\033]7501;??\033\\'
# a report after the query still applies
printf '\033]7501;state=working:progress=50\033\\'
