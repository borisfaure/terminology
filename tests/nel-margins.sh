#!/bin/sh

# clear, DECLRMM on, margins at columns 5 to 20
printf '\033[2J\033[H\033[?69h\033[5;20s'
# NEL inside the margins goes to the left margin
printf '\033[3;10Hab\033Ecd'
# NEL left of the margin goes to column 1, DECOM off
printf '\033[6;2Hef\033Egh'
# NEL with DECOM on: always the left margin
printf '\033[?6h\033[3;3Hij\033Ekl\033[?6l'
# CR left of the margin goes to column 1
printf '\033[10;2Hmn\rop'
# margins off: column 1
printf '\033[?69l\033[12;10Hqr\033Est'
