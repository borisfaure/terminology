#!/bin/sh

# A line feed with the cursor *below* the bottom margin must not scroll the
# scrolling region: the cursor is already outside it, so it just stays put.
# This is the layout tmux uses -- pane rows inside the region, status line on
# the last row underneath it.

# fill the screen so any stray scroll is visible
printf '\033#8'

# pane content
printf '\033[1;1HTOP'
printf '\033[23;1HBOTTOM_OF_REGION'

# status line, below the region
printf '\033[24;1HSTATUS'

# tmux's pane scrolling region: rows 1..23
printf '\033[1;23r'

# park the cursor on the status row and feed lines there
printf '\033[24;5H'
printf '\n\n\n'
