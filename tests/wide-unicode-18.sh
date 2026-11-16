#!/bin/sh

# code points wide since Unicode 16 or later, each followed by a bar that
# lands two cells further

printf '\033[2J\033[H'

# trigram and digram symbols
printf '\342\230\260|\r\n'
printf '\342\232\212|\r\n'
# tai xuan jing symbols
printf '\360\235\214\200|\r\n'
# geometric shapes extended
printf '\360\237\237\232|\r\n'
# cjk unified ideographs extension J
printf '\360\277\260\277|\r\n'
