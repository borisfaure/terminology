#!/bin/sh

# OSC 110, 111 and 112 reset the dynamic colours to the configured ones.
# The tytest defaults are fg aaaaaa, bg 838485 and cursor 878889.

# foreground: set, reset, query
printf '\033]10;#ff0000\007'
printf '\033]110\007'
printf '\033]10;?\007'
# background, ST-terminated
printf '\033]11;#00ff00\033\\'
printf '\033]111\033\\'
printf '\033]11;?\033\\'
# cursor
printf '\033]12;#0000ff\007'
printf '\033]112\007'
printf '\033]12;?\007'
# trailing data is ignored, the reset still happens
printf '\033]12;#0000ff\007'
printf '\033]112;foo\007'
printf '\033]12;?\007'
# one reset leaves the other colours alone
printf '\033]10;#ff0000\007'
printf '\033]11;#00ff00\007'
printf '\033]112\007'
printf '\033]10;?\007'
printf '\033]11;?\007'
printf '\033]110\007\033]111\007'
# resets with nothing to reset are accepted silently
printf '\033]113\007\033]114\007\033]115\007\033]116\007'
printf '\033]117\007\033]118\007\033]119\007'
# reset when nothing was changed
printf '\033]111\007'
printf '\033]11;?\007'
# a "default" value resets like OSC 110/111/112 do
printf '\033]12;#0000ff\007'
printf '\033]12;default\007'
printf '\033]12;?\007'
# all three, with an ESC \ terminator as opentui may send it
printf '\033]10;#ff0000\007\033]11;#00ff00\007\033]12;#0000ff\007'
printf '\033]10;default\033\\\033]11;default\033\\\033]12;default\033\\'
printf '\033]10;?\007\033]11;?\007\033]12;?\007'
# a near miss is still an invalid colour
printf '\033]12;#0000ff\007\033]12;defaults\007'
printf '\033]12;?\007'
