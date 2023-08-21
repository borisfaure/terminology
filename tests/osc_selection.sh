#!/bin/sh

# fill space with E
printf '\033[69;1;1;25;80\044x'

#set color
printf '\033[46;31;3m'

# List of base64 strings used in this test:
# "foobarqux" => Zm9vYmFycXV4Cg==
#"Terminology rox" => VGVybWlub2xvZ3kgcm94Cg==
#"testing!!!!!" => dGVzdGluZyEhISEhCg==
#
#
#set foobarqux as primary selection
printf "\033]52;p;Zm9vYmFycXV4Cg==\033\\"
#set 'testing!!!!!' as clipboard selection
printf "\033]52;c;dGVzdGluZyEhISEhCg==\033\\"
sleep 0.2


# move
printf '\033[3;3H'
# query primary
printf "\033]52;p;?\033\\"
sleep 0.2

# move
printf '\033[5;3H'
# query clipboard
printf "\033]52;c;?\033\\"
sleep 0.2

# move
printf '\033[7;3H'
# query primary, not explicit
printf "\033]52;;?\033\\"
sleep 0.2

# reset primary
printf "\033]52;p;\033\\"
#set 'testing!!!!!' as clipboard selection
printf "\033]52;c;dGVzdGluZyEhISEhCg==\033\\"
sleep 0.2
# move
printf '\033[9;3H'
# query primary then clipboard
printf "\033]52;pc;?\033\\"
