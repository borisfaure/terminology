#!/bin/sh

# Regression test for color 9 confused with COL_INVIS
# (cf https://github.com/borisfaure/terminology/issues/95)
printf '\033[38;5;9mhello\033[0m\n'

# By using the window manipulation sequence, we induce "termpty_resize
# (termpty.c:1339)" where "termpty_line_length (termpty.c:949)" is used .
# "termpty_line_length" uses "_termpty_cell_is_empty" that had a bug.
printf '\033[8;25;132t'
printf '\033[8;25;80t'
