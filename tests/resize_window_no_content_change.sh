#!/bin/sh

# char width: 7
# char height: 15

# clear screen
printf '\033[2J'

# set color
printf '\033[46;31;3m'

printf '\033[2;0Halpha'
printf '\033[2;25Hbravo'

printf '\033[3;0Hcharlie'
printf '\033[3;25Hdela'

printf '\033[4;0Hecho'
printf '\033[4;25Hfoxtrot'

printf '\033[5;0Hgolf'
printf '\033[5;25Hhotel'

printf '\033[6;0Hindia'
printf '\033[6;25Hjuliett'

printf '\033[7;0Hkilo'
printf '\033[7;25Hlima'

printf '\033[8;0Hmike'
printf '\033[8;25Hnovemger'

printf '\033[9;0Hoscar'
printf '\033[9;25Hpap'

printf '\033[10;0Hquebec'
printf '\033[10;25Hromeo'

printf '\033[11;0Hsierra'
printf '\033[11;25Htango'

printf '\033[12;0Huniform'
printf '\033[12;25Hvictor'

printf '\033[13;0Hwhiskey'
printf '\033[13;25Hxray'

printf '\033[14;0Hyankee'
printf '\033[14;25Hzulu'

# force render
printf '\033}tr\0'

#resize window to half width (shall display the same)
printf '\033[8;40;;t'

# force render
printf '\033}tr\0'
