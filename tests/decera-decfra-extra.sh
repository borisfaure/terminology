#!/bin/sh


# fill space with E
printf '\033#8'
# set color
printf '\033[46;31;3m'
# goto 80;25 (CUP) and back to 0;0
printf '\033[26;80HZ'
printf '\033[H'

#DECFRA
printf '\033[64;10;10;;$x'
printf '\033[65;;;8;8;$x'
printf '\033[66;6;;150;3$x'
printf '\033[67;;6;9;150$x'
# invalid coordinates
printf '\033[68;30;20;10;10$x'
# invalid characters
printf '\033[;10;10;20;20$x'
printf '\033[31;10;10;20;20$x'
printf '\033[127;10;10;20;20$x'
printf '\033[159;10;10;20;20$x'

#DECERA
printf '\033[15;15;;$z'
printf '\033[;;3;3$z'
printf '\033[6;;150;3$z'
printf '\033[3;20;;150$z'
# invalid coordinates
printf '\033[30;20;10;10$z'
