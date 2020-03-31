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

## surrounded by spaces
# move to 3; 2
printf '\033[3;2H'
printf ' foo@bar.001      foo@qux.001 '
# mouse move
printf '\033}tm;27;40\0'
# email detection on 'f'
printf '\033}tle;2;2;12;2;foo@bar.001\0'
# mouse move
printf '\033}tm;170;40\0'
# email detection on 'u'
printf '\033}tle;19;2;29;2;foo@qux.001\0'


## surrounded by double quotes
# move to 4; 2
printf '\033[4;2H'
printf '"foo@bar.002"    "foo@qux.002"'
# mouse move
printf '\033}tm;27;55\0'
# email detection on 'f'
printf '\033}tle;2;3;12;3;foo@bar.002\0'
# mouse move
printf '\033}tm;170;55\0'
# email detection on 'u'
printf '\033}tle;19;3;29;3;foo@qux.002\0'


## surrounded by single quotes
# move to 5; 2
printf '\033[5;2H'
printf '\047foo@bar.003\047    \047foo@qux.003\047'
# mouse move
printf '\033}tm;27;70\0'
# email detection on 'f'
printf '\033}tle;2;4;12;4;foo@bar.003\0'
# mouse move
printf '\033}tm;170;70\0'
# email detection on 'u'
printf '\033}tle;19;4;29;4;foo@qux.003\0'


## surrounded by backticks
# move to 6; 2
printf '\033[6;2H'
printf '\140foo@bar.004\140    \140foo@qux.004\140'
# mouse move
printf '\033}tm;27;85\0'
# email detection on 'f'
printf '\033}tle;2;5;12;5;foo@bar.004\0'
# mouse move
printf '\033}tm;170;85\0'
# email detection on 'u'
printf '\033}tle;19;5;29;5;foo@qux.004\0'


## surrounded by angle brackets
# move to 7; 2
printf '\033[7;2H'
printf '<foo@bar.005>    <foo@qux.005>'
# mouse move
printf '\033}tm;27;100\0'
# email detection on 'f'
printf '\033}tle;2;6;12;6;foo@bar.005\0'
# mouse move
printf '\033}tm;170;100\0'
# email detection on 'u'
printf '\033}tle;19;6;29;6;foo@qux.005\0'


## surrounded by square brackets
# move to 8; 2
printf '\033[8;2H'
printf '[foo@bar.006]    [foo@qux.006]'
# mouse move
printf '\033}tm;27;115\0'
# email detection on 'f'
printf '\033}tle;2;7;12;7;foo@bar.006\0'
# mouse move
printf '\033}tm;170;115\0'
# email detection on 'u'
printf '\033}tle;19;7;29;7;foo@qux.006\0'


## surrounded by curly brackets
# move to 9; 2
printf '\033[9;2H'
printf '{foo@bar.007}    {foo@qux.007}'
# mouse move
printf '\033}tm;27;130\0'
# email detection on 'f'
printf '\033}tle;2;8;12;8;foo@bar.007\0'
# mouse move
printf '\033}tm;170;130\0'
# email detection on 'u'
printf '\033}tle;19;8;29;8;foo@qux.007\0'


## surrounded by parentheses
# move to 10; 2
printf '\033[10;2H'
printf '(foo@bar.008)    (foo@qux.008)'
# mouse move
printf '\033}tm;27;145\0'
# email detection on 'f'
printf '\033}tle;2;9;12;9;foo@bar.008\0'
# mouse move
printf '\033}tm;170;145\0'
# email detection on 'u'
printf '\033}tle;19;9;29;9;foo@qux.008\0'


## surrounded by pipes
# move to 11; 2
printf '\033[11;2H'
printf '|foo@bar.009|    |foo@qux.009|'
# mouse move
printf '\033}tm;27;160\0'
# email detection on 'f'
printf '\033}tle;2;10;12;10;foo@bar.009\0'
# mouse move
printf '\033}tm;170;160\0'
# email detection on 'u'
printf '\033}tle;19;10;29;10;foo@qux.009\0'
exit 0
