#!/bin/sh

# reset screen
printf '\033[2J'
# set color
printf '\033[46;31;3m'

# move to 2; 0
printf '\033[2;2H'

# positions when over links
printf '  o                    u      '
printf '\033}td;27;25;1;0;1\0'
printf '\033}tu;27;25;1;0;1\0'
printf '\033}td;27;25;1;0;1\0'
printf '\033}tu;27;25;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tso\0'
printf '\033}td;170;26;1;0;1\0'
printf '\033}tu;170;26;1;0;1\0'
printf '\033}td;170;26;1;0;1\0'
printf '\033}tu;170;26;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsu\0'

## surrounded by angle brackets, normal ones
# move to 3; 2
printf '\033[3;2H'
printf '<foo@bar.001>    <foo@qux.001>'
# mouse move
printf '\033}tm;27;40\0'
# email detection on 'f'
printf '\033}tle;2;2;12;2;foo@bar.001\0'
# mouse move
printf '\033}tm;170;40\0'
# email detection on 'u'
printf '\033}tle;19;2;29;2;foo@qux.001\0'


## surrounded by french guillemets
# move to 4; 2
printf '\033[4;2H'
printf '«foo@bar.002»    «foo@qux.002»'
# mouse move
printf '\033}tm;27;55\0'
# email detection on 'f'
printf '\033}tle;2;3;12;3;foo@bar.002\0'
# mouse move
printf '\033}tm;170;55\0'
# email detection on 'u'
printf '\033}tle;19;3;29;3;foo@qux.002\0'


## surrounded by inverted square brackets
# move to 5; 2
printf '\033[5;2H'
printf ']foo@bar.003[    ]foo@qux.003['
# mouse move
printf '\033}tm;27;70\0'
# email detection on 'f'
printf '\033}tle;2;4;12;4;foo@bar.003\0'
# mouse move
printf '\033}tm;170;70\0'
# email detection on 'u'
printf '\033}tle;19;4;29;4;foo@qux.003\0'


## surrounded by small guillemets
# move to 6; 2
printf '\033[6;2H'
printf '‹foo@bar.004›    ‹foo@qux.004›'
# mouse move
printf '\033}tm;27;85\0'
# email detection on 'f'
printf '\033}tle;2;5;12;5;foo@bar.004\0'
# mouse move
printf '\033}tm;170;85\0'
# email detection on 'u'
printf '\033}tle;19;5;29;5;foo@qux.004\0'


## surrounded by pointy brackets
# move to 7; 2
printf '\033[7;2H'
printf '⟨foo@bar.005⟩    ⟨foo@qux.005⟩'
# mouse move
printf '\033}tm;27;100\0'
# email detection on 'f'
printf '\033}tle;2;6;12;6;foo@bar.005\0'
# mouse move
printf '\033}tm;170;100\0'
# email detection on 'u'
printf '\033}tle;19;6;29;6;foo@qux.005\0'


## surrounded by double square brackets
# move to 8; 2
printf '\033[8;2H'
printf '⟦foo@bar.006⟧    ⟦foo@qux.006⟧'
# mouse move
printf '\033}tm;27;115\0'
# email detection on 'f'
printf '\033}tle;2;7;12;7;foo@bar.006\0'
# mouse move
printf '\033}tm;170;115\0'
# email detection on 'u'
printf '\033}tle;19;7;29;7;foo@qux.006\0'


## surrounded by english quotes
# move to 9; 2
printf '\033[9;2H'
printf '“foo@bar.007”    “foo@qux.007”'
# mouse move
printf '\033}tm;27;130\0'
# email detection on 'f'
printf '\033}tle;2;8;12;8;foo@bar.007\0'
# mouse move
printf '\033}tm;170;130\0'
# email detection on 'u'
printf '\033}tle;19;8;29;8;foo@qux.007\0'

## surrounded by swedish guillemets
# move to 10; 2
printf '\033[10;2H'
printf '»foo@bar.008«    »foo@qux.008«'
# mouse move
printf '\033}tm;27;145\0'
# email detection on 'f'
printf '\033}tle;2;9;12;9;foo@bar.008\0'
# mouse move
printf '\033}tm;170;145\0'
# email detection on 'u'
printf '\033}tle;19;9;29;9;foo@qux.008\0'


## surrounded by english single quotes
# move to 11; 2
printf '\033[11;2H'
printf '‘foo@bar.009’    ‘foo@qux.009’'
# mouse move
printf '\033}tm;27;160\0'
# email detection on 'f'
printf '\033}tle;2;10;12;10;foo@bar.009\0'
# mouse move
printf '\033}tm;170;160\0'
# email detection on 'u'
printf '\033}tle;19;10;29;10;foo@qux.009\0'


## surrounded by english single quotes, first reversed
# move to 12; 2
printf '\033[12;2H'
printf '‛foo@bar.010’    ‛foo@qux.010’'
# mouse move
printf '\033}tm;27;175\0'
# email detection on 'f'
printf '\033}tle;2;11;12;11;foo@bar.010\0'
# mouse move
printf '\033}tm;170;175\0'
# email detection on 'u'
printf '\033}tle;19;11;29;11;foo@qux.010\0'


## surrounded by german quotes
# move to 13; 2
printf '\033[13;2H'
printf '„foo@bar.011“    „foo@qux.011“'
# mouse move
printf '\033}tm;27;190\0'
# email detection on 'f'
printf '\033}tle;2;12;12;12;foo@bar.011\0'
# mouse move
printf '\033}tm;170;190\0'
# email detection on 'u'
printf '\033}tle;19;12;29;12;foo@qux.011\0'


## surrounded by polish quotes
# move to 14; 2
printf '\033[14;2H'
printf '„foo@bar.012”    „foo@qux.012”'
# mouse move
printf '\033}tm;27;205\0'
# email detection on 'f'
printf '\033}tle;2;13;12;13;foo@bar.012\0'
# mouse move
printf '\033}tm;170;205\0'
# email detection on 'u'
printf '\033}tle;19;13;29;13;foo@qux.012\0'


## surrounded by pointing angle brackets
# move to 15; 2
printf '\033[15;2H'
printf '〈foo@bar.013〉    〈foo@qux.013〉'
# mouse move
printf '\033}tm;27;220\0'
# email detection on 'f'
printf '\033}tle;3;14;13;14;foo@bar.013\0'
# mouse move
printf '\033}tm;170;220\0'
# email detection on 'u'
printf '\033}tle;22;14;32;14;foo@qux.013\0'

### corners
# positions when over links
# move to 2; 40
printf '\033[2;40H'
printf '  o                    u      '
printf '\033}td;292;25;1;0;1\0'
printf '\033}tu;292;25;1;0;1\0'
printf '\033}td;292;25;1;0;1\0'
printf '\033}tu;292;25;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tso\0'
printf '\033}td;439;25;1;0;1\0'
printf '\033}tu;439;25;1;0;1\0'
printf '\033}td;439;25;1;0;1\0'
printf '\033}tu;439;25;1;0;1\0'
# force render
printf '\033}tr\0'
# selection is
printf '\033}tsu\0'

## surrounded by corners
# move to 3; 40
printf '\033[3;40H'
printf '⌜foo@bar.014⌝    ⌜foo@qux.014⌝'
# mouse move
printf '\033}tm;292;40\0'
# email detection on 'f'
printf '\033}tle;40;2;50;2;foo@bar.014\0'
# mouse move
printf '\033}tm;439;40\0'
# email detection on 'u'
printf '\033}tle;57;2;67;2;foo@qux.014\0'


## surrounded by corners
# move to 4; 40
printf '\033[4;40H'
printf '⌜foo@bar.015⌟    ⌜foo@qux.015⌟'
# mouse move
printf '\033}tm;292;55\0'
# email detection on 'f'
printf '\033}tle;40;3;50;3;foo@bar.015\0'
# mouse move
printf '\033}tm;439;55\0'
# email detection on 'u'
printf '\033}tle;57;3;67;3;foo@qux.015\0'


## surrounded by corners
# move to 5; 40
printf '\033[5;40H'
printf '⌞foo@bar.016⌝    ⌞foo@qux.016⌝'
# mouse move
printf '\033}tm;292;70\0'
# email detection on 'f'
printf '\033}tle;40;4;50;4;foo@bar.016\0'
# mouse move
printf '\033}tm;439;70\0'
# email detection on 'u'
printf '\033}tle;57;4;67;4;foo@qux.016\0'


## surrounded corners
# move to 6; 40
printf '\033[6;40H'
printf '⌞foo@bar.017⌟    ⌞foo@qux.017⌟'
# mouse move
printf '\033}tm;292;85\0'
# email detection on 'f'
printf '\033}tle;40;5;50;5;foo@bar.017\0'
# mouse move
printf '\033}tm;439;85\0'
# email detection on 'u'
printf '\033}tle;57;5;67;5;foo@qux.017\0'


## surrounded by lengthy corners
# move to 7; 40
printf '\033[7;40H'
printf '⌈foo@bar.018⌉    ⌈foo@qux.018⌉'
# mouse move
printf '\033}tm;292;100\0'
# email detection on 'f'
printf '\033}tle;40;6;50;6;foo@bar.018\0'
# mouse move
printf '\033}tm;439;100\0'
# email detection on 'u'
printf '\033}tle;57;6;67;6;foo@qux.018\0'


## surrounded by lengthy corners
# move to 8; 40
printf '\033[8;40H'
printf '⌈foo@bar.019⌋    ⌈foo@qux.019⌋'
# mouse move
printf '\033}tm;292;115\0'
# email detection on 'f'
printf '\033}tle;40;7;50;7;foo@bar.019\0'
# mouse move
printf '\033}tm;439;115\0'
# email detection on 'u'
printf '\033}tle;57;7;67;7;foo@qux.019\0'


## surrounded by lengthy corners
# move to 9; 40
printf '\033[9;40H'
printf '⌊foo@bar.020⌉    ⌊foo@qux.020⌉'
# mouse move
printf '\033}tm;292;130\0'
# email detection on 'f'
printf '\033}tle;40;8;50;8;foo@bar.020\0'
# mouse move
printf '\033}tm;439;130\0'
# email detection on 'u'
printf '\033}tle;57;8;67;8;foo@qux.020\0'

## surrounded by lengthy corners
# move to 10; 40
printf '\033[10;40H'
printf '⌊foo@bar.021⌋    ⌊foo@qux.021⌋'
# mouse move
printf '\033}tm;292;145\0'
# email detection on 'f'
printf '\033}tle;40;9;50;9;foo@bar.021\0'
# mouse move
printf '\033}tm;439;145\0'
# email detection on 'u'
printf '\033}tle;57;9;67;9;foo@qux.021\0'
