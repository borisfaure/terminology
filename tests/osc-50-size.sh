#!/bin/sh

# OSC 50 font size: each :size= match must not run the scan past the end
# of the string
printf '\033]50;:size=:size=:size=:size=:size=:size=:size=:size=\007'
printf '\033]50;font:size=12:size=:size=:size=\033\\'
printf '\033]50;:size=\007'
printf '\033]50;:size=:size=x\007'

printf 'done'
