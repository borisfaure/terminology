#!/bin/sh

# cursor to 0,0
printf '\033[H'
printf '\033[0m(256) System colors: background\n'
for COLOR in $(seq 0 7); do
   printf '\033[48;5;%sm  ' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 8 15); do
   printf '\033[48;5;%sm  ' "${COLOR}"
done
printf '\033[0m\n';

printf '\033[0m System colors: background\n'
for COLOR in $(seq 40 47); do
   printf '\033[%sm  ' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 100 107); do
   printf '\033[%sm  ' "${COLOR}"
done
printf '\033[0m\n';


printf '\033[0m System colors: foreground\n'
for COLOR in $(seq 30 37); do
   printf '\033[0;%sm██' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 90 97); do
   printf '\033[0;%sm██' "${COLOR}"
done
printf '\033[0m\n';

printf '\n\n\n\nColor cube, 6x6x6:\n';
for GREEN in $(seq 0 5); do
   for RED in $(seq 0 5); do
      for BLUE in $(seq 0 5); do
         COLOR=$((16 + (RED * 36) + (GREEN * 6) + BLUE))
         printf '\033[48;5;%sm  ' "${COLOR}"
      done
      printf '\033[0m '
   done
   printf '\n'
done

#printf '\033[0m\n';
printf 'Grayscale ramp:\n';
for COLOR in $(seq 232 255); do
    printf '\033[48;5;%sm  ' "${COLOR}"
done
printf '\x1b[0m\n';

# restrict cursor
printf '\033[?6h'
# allow left/right margins
printf '\033[?69h'
# set left/right margins:
printf '\033[40s'


printf '\033[0m Bright colors: background\n'
for COLOR in $(seq 40 47); do
   printf '\033[1;%sm  ' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 100 107); do
   printf '\033[1;%sm  ' "${COLOR}"
done
printf '\033[0m\n';


printf '\033[0m Bright colors: foreground\n'
for COLOR in $(seq 30 37); do
   printf '\033[1;%sm██' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 90 97); do
   printf '\033[1;%sm██' "${COLOR}"
done
printf '\033[0m\n';


printf '\033[0m Dim/Faint colors: background\n'
for COLOR in $(seq 40 47); do
   printf '\033[2;%sm  ' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 100 107); do
   printf '\033[2;%sm  ' "${COLOR}"
done
printf '\033[0m\n';


printf '\033[0m Dim/Faint colors: foreground\n'
for COLOR in $(seq 30 37); do
   printf '\033[2;%sm██' "${COLOR}"
done
printf '\033[0m\n';
for COLOR in $(seq 90 97); do
   printf '\033[2;%sm██' "${COLOR}"
done
printf '\033[0m\n';
