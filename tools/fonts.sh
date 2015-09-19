#!/bin/sh
echo -ne "normal\n"
echo -ne "\033[0mabcdefghijklmnopqrstuvwxyz\n\033[0m"
echo -ne "\033[0mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m"

echo -ne "\nbold\n"
echo -ne "\033[1mabcdefghijklmnopqrstuvwxyz\n\033[0m"
echo -ne "\033[1mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m"

echo -ne "\nitalic\n"
echo -ne "\033[3mabcdefghijklmnopqrstuvwxyz\n\033[0m"
echo -ne "\033[3mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m"

echo -ne "\nbolditalic\n"
echo -ne "\033[3;1mabcdefghijklmnopqrstuvwxyz\n\033[0m"
echo -ne "\033[3;1mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m"

echo -ne "\nfraktur\n"
echo -ne "\033[20mabcdefghijklmnopqrstuvwxyz\n\033[0m"
echo -ne "\033[20mABCDEFGHIJKLMNOPQRSTUVWXYZ\n\033[0m"
