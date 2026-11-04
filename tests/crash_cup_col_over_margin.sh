#!/bin/sh
# CUP/HVP with a column or row at int's ceiling, after DECSLRM gave the
# cursor arithmetic a nonzero left/top margin to add: the margin add
# overflowed into a negative cursor, which slipped past the upper-bound
# clamps and the next write went through a wild cells pointer.
#
# Edge cases: the exact overflow value (2147483648 wraps to negative when
# parsed), the largest legal value (2147483647 has to clamp into the
# margins, not overflow), same without origin mode, and missing
# parameters.
printf '\033[?69h\033[15;30s\033[?6h\033[5;2147483648H'
printf 'X'
printf '\033[0;16H'
printf 'Y'
printf '\033[5;2147483647H'
printf 'Z'
printf '\033[2147483648;2147483647H'
printf 'W'
printf '\033[?6l\033[5;2147483648H'
printf 'V'
printf '\033[;2147483648H'
printf 'U'
printf '\033[2147483648H'
printf 'T'
printf 'UVT'
