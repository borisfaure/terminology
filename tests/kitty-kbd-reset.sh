#!/bin/sh
# kitty keyboard protocol: RIS and DECSTR clear both stacks
printf '\033[>1u\033[?1049h\033[>1u'
# DECSTR
printf '\033[!p\033[?u'
printf '\033[?1049l\033[?u'
printf '\033[>1u\033[?1049h\033[>1u\033[?1049l'
# RIS
printf '\033c\033[?u'
printf '\033[?1049h\033[?u\033[?1049l'
