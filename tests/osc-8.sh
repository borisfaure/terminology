#!/bin/sh

# OSC 8 hyperlinks: open, dereference-free close, and the malformed shapes
# that must all land in a deterministic state, leaks included.

# opening with a URI, closed by an empty one
printf '\033]8;id=home;http://terminology.example/\033\\'
printf 'home page\033]8;;\033\\'

# same, but opened and closed with BEL
printf '\033]8;id=character;http://terminology.example/#main\-char\007'
printf 'characters\033]8;;\007'

# no id parameter
printf '\033]8;;http://terminology.example/plain\033\\'
printf 'plain link\033]8;;\033\\'

# several parameters, one with a colon, as some apps emit
printf '\033]8;id=x:y;foo=bar;http://terminology.example/params\033\\'
printf 'parameters\033]8;\033\\'

# a close without an open logs an error and changes nothing
printf '\033]8;;\033\\'

# a second close while none is pending logs an error too
printf '\033]8;id=one;http://a.example/1\033\\\033]8;;\033\\\033]8;;\033\\'

# a missing ';' separator is an error; empty parameter parts are not
printf '\033]8;nosemi http://terminology.example/nosemi\033\\\033]8;;\033\\'

# an empty URL and a payload with nothing after it
printf '\033]8;id=empty;\033\\\033]8;;\033\\'
printf '\033]8;\033\\'
