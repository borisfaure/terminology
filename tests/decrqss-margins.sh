#!/bin/sh

printf '\033P$qr\033\\'
printf '\033[5;10r\033P$qr\033\\'
printf '\033[5;24r\033P$qr\033\\'
printf '\033[r\033P$qr\033\\'
printf '\033P$qs\033\\'
printf '\033[?69h\033[5;10s\033P$qs\033\\'
printf '\033[?69l\033P$qs\033\\'
printf '\033[3;20r\033[?69h\033[2;40s\033c\033P$qr\033\\\033P$qs\033\\'
