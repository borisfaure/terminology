#!/bin/sh
printf 'normal\n'
printf '\033[0mabcdefghijklmnopqrstuvwxyz\n\033[0m'
printf '\033[0mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m'

printf '\nbold\n'
printf '\033[1mabcdefghijklmnopqrstuvwxyz\n\033[0m'
printf '\033[1mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m'

printf '\nitalic\n'
printf '\033[3mabcdefghijklmnopqrstuvwxyz\n\033[0m'
printf '\033[3mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m'

printf '\nbolditalic\n'
printf '\033[3;1mabcdefghijklmnopqrstuvwxyz\n\033[0m'
printf '\033[3;1mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m'

printf '\nfraktur\n'
printf '\033[20mabcdefghijklmnopqrstuvwxyz\n\033[0m'
printf '\033[20mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m'

printf '\nencircled\n'
printf '\033[52mabcdefghijklmnopqrstuvwxyz01234\n\033[0m'
printf '\033[52mABCDEFGHIJKLMNOPQRSTUVWXYZ56789\n\033[0m'
