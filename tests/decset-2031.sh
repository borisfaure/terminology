#!/bin/sh

# DEC private mode 2031, colour scheme change reporting, and its DECRQM.
#
# Expected write buffer:
#   \e[?2031;2$y  reset by default
#   \e[?2031;1$y  ?2031h sends nothing by itself, only the DECRQM answers
#   \e[?2031;1$y  DECSTR keeps the mode
#   \e[?2031;2$y  ?2031l
#   \e[O          ?1004h reports the focus out (tytest stub: not focused)
#   \e[?2031;1$y  ?1004;2031h set both modes
#   \e[?2031;2$y  RIS clears the mode
#   \e[?997;2n    ?996n still answers with the mode reset: the fake
#                 background 131/132/133 is light

# reset by default
printf '\033[?2031$p'
# setting sends nothing by itself
printf '\033[?2031h'
printf '\033[?2031$p'
# DECSTR keeps it
printf '\033[!p'
printf '\033[?2031$p'
printf '\033[?2031l'
printf '\033[?2031$p'
# several modes in one sequence
printf '\033[?1004;2031h'
printf '\033[?2031$p'
# RIS clears it
printf '\033c'
printf '\033[?2031$p'
# the query still answers with the mode reset
printf '\033[?996n'
