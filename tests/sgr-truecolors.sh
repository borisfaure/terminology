#!/bin/sh

# pick 2 colors as RGB (orange for background, blue for foreground)
# compute the values for CMY and CMYK colorspaces
# Have 4 colums for each kind of format used in TrueColor escape codes

#BG-48: #f49019
# R:244 G:144 B:25
# C:4 M:43 Y:90
# C:0 M:41 Y:90 K:4

#FG-38: #3896c7
# R:56 G:150 B:199
# C:78 M:41 Y:22
# C:72 M:25 Y:0 K:22


# cursor to 0,0 and clear line
printf '\033[H\033[2K'

# formats for each columns
printf '\033[1;1H\033[0m38:2:n:n:nm'
printf '\033[1;13H\033[0m38:2:42:n:n:nm'
printf '\033[1;28H\033[0m38:2:42:n:n:n:4m'
printf '\033[1;45H\033[0m38;2;n;n;nm'

##
# RGB
##
printf '\033[3;1H\033[0;1;37mRGB'

# 1
printf '\033[4;1H\033[0;1;37m1'
printf '\033[48:2:244:144:25m'
printf '\033[38:2:56:150:199m'
printf '\033[4;5H▗▖'
printf '\033[5;5H▝▘'

# 2
printf '\033[4;13H\033[0;1;37m2'
printf '\033[48:2:42:244:144:25m'
printf '\033[38:2:42:56:150:199m'
printf '\033[4;17H▗▖'
printf '\033[5;17H▝▘'

# 3
printf '\033[4;28H\033[0;1;37m3'
printf '\033[48:2:42:244:144:25:4m'
printf '\033[38:2:42:56:150:199:4m'
printf '\033[4;32H▗▖'
printf '\033[5;32H▝▘'

# 4
printf '\033[4;45H\033[0;1;37m4'
printf '\033[48;2;244;144;25m'
printf '\033[38;2;56;150;199m'
printf '\033[4;49H▗▖'
printf '\033[5;49H▝▘'

# Same but on one sequence
printf '\033[6;1H\033[0mSame but fg+bg on one sequence'

#1
printf '\033[7;1H\033[0;1;37m1'
printf '\033[48:2:244:144:25;38:2:56:150:199m'
printf '\033[7;5H▗▖'
printf '\033[8;5H▝▘'

# 2
printf '\033[7;13H\033[0;1;37m2'
printf '\033[1;13H\033[0m38:2:42:n:n:nm'
printf '\033[48:2:42:244:144:25;38:2:42:56:150:199m'
printf '\033[7;17H▗▖'
printf '\033[8;17H▝▘'

# 3
printf '\033[7;28H\033[0;1;37m3'
printf '\033[48:2:42:244:144:25:4;38:2:42:56:150:199:4m'
printf '\033[7;32H▗▖'
printf '\033[8;32H▝▘'

# 4
printf '\033[7;45H\033[0;1;37m4'
printf '\033[48;2;244;144;25;38;2;56;150;199m'
printf '\033[7;49H▗▖'
printf '\033[8;49H▝▘'


##
# CMY
##
printf '\033[10;1H\033[0;1;37mCMY'
# 1
printf '\033[11;1H\033[0;1;37m1'
printf '\033[48:3:4:43:90m'
printf '\033[38:3:78:41:22m'
printf '\033[11;5H▗▖'
printf '\033[12;5H▝▘'

# 2
printf '\033[11;13H\033[0;1;37m2'
printf '\033[48:3:42:4:43:90m'
printf '\033[38:3:42:78:41:22m'
printf '\033[11;17H▗▖'
printf '\033[12;17H▝▘'

# 3
printf '\033[11;28H\033[0;1;37m3'
printf '\033[48:3:42:4:43:90:4m'
printf '\033[38:3:42:78:41:22:4m'
printf '\033[11;32H▗▖'
printf '\033[12;32H▝▘'

# 4
printf '\033[11;45H\033[0;1;37m4'
printf '\033[48;3;4;43;90m'
printf '\033[38;3;78;41;22m'
printf '\033[11;49H▗▖'
printf '\033[12;49H▝▘'


##
# CMYK
##
printf '\033[14;1H\033[0;1;37mCMYK'
# 1
printf '\033[15;1H\033[0;1;37m1'
printf '\033[48:4::0:41:90:4m'
printf '\033[38:4::72:25:0:22m'
printf '\033[15;5H▗▖'
printf '\033[16;5H▝▘'

# 2
printf '\033[15;13H\033[0;1;37m2'
printf '\033[48:4:42:0:41:90:4m'
printf '\033[38:4:42:72:25:0:22m'
printf '\033[15;17H▗▖'
printf '\033[16;17H▝▘'

# 3
printf '\033[15;28H\033[0;1;37m3'
printf '\033[48:4:42:0:41:90:4:4m'
printf '\033[38:4:42:72:25:0:22:4m'
printf '\033[15;32H▗▖'
printf '\033[16;32H▝▘'

# 4
printf '\033[15;45H\033[0;1;37m4'
printf '\033[48;4;0;41;90;4m'
printf '\033[38;4;72;25;0;22m'
printf '\033[15;49H▗▖'
printf '\033[16;49H▝▘'
