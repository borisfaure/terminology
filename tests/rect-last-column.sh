#!/bin/sh

# clear screen
printf '\033[2J\033[H'

# DECFRA, DECCARA and DECERA starting on the last column or row only
printf '\033[88;1;80;24;80$x'
printf '\033[89;24;1;24;80$x'
printf '\033[1;80;24;80;1$r'
printf '\033[23;79;24;80${'
printf '\033[23;1;24;1$z'

# DECCRA from and to the last column
printf '\033[1;80;3;80;1;5;80;1$v'

# on a single column, every rectangle starts on the last column
printf '\033[8;5;1t'
printf '\033[88;1;1;5;1$x'
printf '\033[1;1;5;1;7$r'
printf '\033[2;1;3;1$z'
printf '\033[1;1;2;1;1;4;1;1$v'

printf '\033[8;24;80t'
