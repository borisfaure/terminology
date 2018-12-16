#!/bin/sh

# long sequence
printf '\033[0;1;2;3;4;5;9;20;38;5;58;48;5;34;52;53mo'
# long sequence with 98/108 which are not allowed, were producing the same
printf '\033[0;1;2;3;4;5;9;20;98;5;58;108;5;34;52;53mo'
printf '\n'
# both shall provide the same output
printf '\033[0;5;58;5;34mo\033[0;98;5;58;108;5;34mo'

