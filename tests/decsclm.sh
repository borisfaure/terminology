#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80$x'

# set scrolling mode to fast
printf '\033[?4l'
# set scrolling mode to smooth
printf '\033[?4h'

