#!/bin/sh

# fill space with E
printf '\033#8'

# two ids interleaved, as behind a multiplexer
printf '\033]99;i=a:d=0;first \033\\'
printf '\033]99;i=b:d=0;second \033\\'
printf '\033]99;i=a:d=0;title\033\\'
printf '\033]99;i=b;title\033\\'
printf '\033]99;i=a;\033\\'
# base64 cut after encoding, in the middle of a quantum
printf '\033]99;i=c:e=1:d=0;SGVsbG\033\\'
printf '\033]99;i=c:e=1;8gd29ybGQ=\033\\'
# base64 cut before encoding, each chunk padded
printf '\033]99;i=d:e=1:d=0;SGVs\033\\'
printf '\033]99;i=d:e=1;bG8=\033\\'
# unpadded last chunk
printf '\033]99;e=1;SGVsbG8gd29ybGQ\033\\'
# invalid base64: that chunk is dropped
printf '\033]99;i=e:e=1:d=0;!!!!\033\\'
printf '\033]99;i=e;fallback\033\\'
# base64 of invalid UTF-8 and of a control character: dropped
printf '\033]99;e=1;gP8=\033\\'
printf '\033]99;e=1;YQdi\033\\'
# a UTF-8 character split across two base64 chunks survives
printf '\033]99;i=s:e=1:d=0;YcO\033\\'
printf '\033]99;i=s:e=1;p\033\\'
# a newline is kept in the body, becomes a space in the title
printf '\033]99;i=n:p=title:e=1:d=0;bGluZTEKbGluZTI=\033\\'
printf '\033]99;i=n:p=body:e=1;bGluZTEKbGluZTI=\033\\'
# a tab in plain text becomes a space
printf '\033]99;;tab\there\033\\'
# a bad id is ignored: both chunks are id-less, and merge
printf '\033]99;i=b$d:d=0;not\033\\'
printf '\033]99;i=b$d;merged\033\\'
# nine incomplete notifications: the first one is evicted
for n in 1 2 3 4 5 6 7 8 9; do
   printf '\033]99;i=p%s:d=0;pending %s\033\\' "$n" "$n"
done
printf '\033]99;i=p1;\033\\'
printf '\033]99;i=p9;\033\\'
# a title longer than the limit is cut at 1024 bytes, in three chunks
printf '\033]99;i=t:d=0;'
i=0
while [ $i -lt 400 ]; do printf 'a'; i=$((i+1)); done
printf '\033\\'
printf '\033]99;i=t:d=0;'
i=0
while [ $i -lt 400 ]; do printf 'a'; i=$((i+1)); done
printf '\033\\'
printf '\033]99;i=t;'
i=0
while [ $i -lt 300 ]; do printf 'a'; i=$((i+1)); done
printf '\033\\'
