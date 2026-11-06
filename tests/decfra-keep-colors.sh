#!/bin/sh

# DECFRA under default colours keeps each cell's colours and takes the
# other attributes; under set colours it replaces them.

# reset screen
printf '\033[2J\033[1;1H'

# coloured areas: palette, bright, 256 and RGB colours, faint and italic
printf '\033[0;31;42;2m\033[35;1;1;6;20\044x'
printf '\033[0;91;104;3m\033[36;7;1;12;20\044x'
printf '\033[0;38;5;200;48;5;17m\033[37;1;21;6;40\044x'
printf '\033[0;38;2;10;20;30;48;2;200;100;50m\033[38;7;21;12;40\044x'

# default colours, bold and underline, across all four
printf '\033[0;1;4m\033[61;4;10;9;30\044x'

# default colours, only the inverse attribute
printf '\033[0;7m\033[62;2;35;10;38\044x'

# hyperlinked text, then filled under default then set colours
printf '\033[0m\033[14;1H\033]8;;http://example.com/a\033\\link a link a\033]8;;\033\\'
printf '\033[15;1H\033[33m\033]8;id=b;http://example.com/b\033\\link b link b\033]8;;\033\\'
printf '\033[0m\033[63;14;3;15;8\044x'
printf '\033[35;45m\033[64;14;6;15;12\044x'

printf '\033[0m\033[20;1H'
