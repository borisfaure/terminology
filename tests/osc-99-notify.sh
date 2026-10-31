#!/bin/sh

# fill space with E
printf '\033#8'

# the simplest form: title only, no metadata
printf '\033]99;;Hello world\033\\'
# the spec's title then body
printf '\033]99;i=1:d=0;Hello world\033\\'
printf '\033]99;i=1:p=body;This is cool\033\\'
# what opentui sends: base64 title held back, then the body
printf '\033]99;i=opentui-1:p=title:e=1:d=0;T3BlbkNvZGU=\033\\'
printf '\033]99;i=opentui-1:p=body:e=1:d=1;U2Vzc2lvbiBkb25l\033\\'
# body only: becomes the title
printf '\033]99;p=body;only a body\007'
# urgency and expiry are forwarded
printf '\033]99;u=2:w=5000;critical\033\\'
# out of range urgency and timeout: defaults
printf '\033]99;u=7:w=-9;defaults\033\\'
# unknown key ignored, part without "=" skipped
printf '\033]99;x=1:junk:i=2;still shown\033\\'
# unknown payload type: payload ignored, nothing shown
printf '\033]99;p=icon;ignored\033\\'
# empty title and body: nothing shown
printf '\033]99;;\033\\'
# no second semicolon: malformed, nothing shown
printf '\033]99;Hello\033\\'
# C1 OSC and ST
printf '\302\23599;;eight bit\302\234'
