#!/bin/sh

# fill space with E
printf '\033#8'

# opentui's probe, ended with ST: answered with ST
printf '\033]99;i=opentui-notifications:p=?;\033\\'
# ended with BEL: answered with BEL
printf '\033]99;i=q2:p=?;\007'
# no id: i=0
printf '\033]99;p=?;\033\\'
# an id with characters outside the set: not echoed, i=0
printf '\033]99;i=evil$(id):p=?;\033\\'
# key order does not matter
printf '\033]99;p=?:i=late;\033\\'
# a query in the middle of a chunked notification keeps it
printf '\033]99;i=k:d=0;kept \033\\'
printf '\033]99;i=k2:p=?;\033\\'
printf '\033]99;i=k;title\033\\'
# the recommended probe order: query, then DA1
printf '\033]99;i=x:p=?;\033\\\033[c'
