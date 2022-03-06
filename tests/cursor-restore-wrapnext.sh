#!/bin/sh

# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'

# move to start of line 3
printf '\033[3;0H'

# print O
printf 'O'

# save cursor
printf '\e7'

# move to end of line 3
printf '\033[3;80H'

# print A, that sets "wrapnext"
printf 'A'

# restore cursor: "wrapnext" should not be set
printf '\e8'


# print K. must be next to 'O'
printf 'K\n'
