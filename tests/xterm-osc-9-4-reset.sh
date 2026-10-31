#!/bin/sh

# fill space with E
printf '\033#8'

# set 80%, then RIS, then an error report: the value must be forgotten
printf '\033]9;4;1;80\033\\'
printf '\033c'
printf '\033]9;4;2\007'
