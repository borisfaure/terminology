#!/bin/sh

# fill space with E
printf '\033#8'

# set color
printf '\033[46;31;3m'

##
# invalid foreground/background truecolors, separated with ':'
##
# RGB
printf '\033[48:2:244:144:25:>m'
printf '\033[38:2:56:150:199:>m'

# CMY
printf '\033[48:3:4:43:90:>m'
printf '\033[38:3:78:41:22:>m'

# CMYK
printf '\033[48:4::0:41:90:4:>m'
printf '\033[38:4::72:25:0:22:>m'

##
# invalid foreground/background truecolors, separated with ':'
##
# RGB
printf '\033[48;2;244;144;25;>m'
printf '\033[38;2;56;150;199;>m'

# CMY
printf '\033[48;3;4;43;90;>m'
printf '\033[38;3;78;41;22;>m'

# CMYK
printf '\033[48;4;0;41;90;4;>m'
printf '\033[38;4;72;25;0;22;>m'
