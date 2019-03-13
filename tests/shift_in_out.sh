#!/bin/sh

# fill space with E
printf '\033#8'

# move to 0; 0
printf '\033[H'

#set color
printf '\033[46;31;3m'

##
# Shift In
##
printf '\017'
for C in $(seq 32 64); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'
for C in $(seq 64 96); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'
for C in $(seq 96 127); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'

##
# Shift Out
##
printf '\n'
printf '\016'
for C in $(seq 32 64); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'
for C in $(seq 64 96); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'
for C in $(seq 96 127); do
    printf "\x$(printf '%x' $C)"
done
printf '\n'
