#!/bin/sh

# query background color
printf '\033]11;?\007'
sleep 0.1
#should print ^]]11;rgb:83/84/85^G
