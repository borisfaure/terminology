#!/bin/sh
# shellcheck source=tests/utils.sh
. "$(dirname "$0")/utils.sh"

# one known record; every report below must leave the records unchanged
printf '\033]7501;id=keep:state=idle\033\\'
printf '\033]7501;id=x\033\\'                         # no state
printf '\033]7501;id=x:state=sleeping\033\\'          # unknown state
printf '\033]7501;id=a//b:state=idle\033\\'           # bad id
printf '\033]7501;id=a/b/c/d/e/f/g/h/i:state=idle\033\\'  # 9 levels
printf '\033]7501;state=idle:msg=YQpi\033\\'          # newline in msg
printf '\033]7501;state=idle:msg=YQli\033\\'          # tab in msg
printf '\033]7501;state=idle:msg=Q\033\\'             # bad base64
printf '\033]7501\007'                                # no body
printf '\033]7501;\033\\'                             # empty body

# a title over the 256 encoded byte maximum; 260 keeps the base64 shape
# valid, so only the size check can reject it
printf '\033]7501;id=t:state=done:title='
printf '%*s' 260 '' | tr ' ' A
printf '\033\\'

# a msg over the 2732 encoded byte maximum, shape still valid
printf '\033]7501;id=m:state=working:msg='
printf '%*s' 2736 '' | tr ' ' A
printf '\033\\'

# an id segment of 33 bytes, one over the limit
printf '\033]7501;id='
printf '%*s' 33 '' | tr ' ' A
printf ':state=idle\033\\'

# an OSC longer than the 4096 codepoint buffer is discarded up to its ST
printf '\033]7501;id=big:state=working:msg='
printf '%*s' 4100 '' | tr ' ' A
printf '\033\\X'
