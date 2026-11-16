#!/bin/sh

# characters that take no cell, as wcwidth() in glibc: the bar after each
# lands right after the base letter

printf '\033[2J\033[H'

# hebrew point, arabic mark, enclosing circle, combining marks extended
# and supplement, half mark, variation selector supplement
printf 'a\326\260|\r\n'
printf 'a\331\213|\r\n'
printf 'a\342\203\235|\r\n'
printf 'a\341\252\260|\r\n'
printf 'a\341\267\200|\r\n'
printf 'a\357\270\240|\r\n'
printf 'a\363\240\204\200|\r\n'

# format characters: zero width joiner, word joiner, byte order mark,
# bidi embedding
printf 'a\342\200\215|\r\n'
printf 'a\342\201\240|\r\n'
printf 'a\357\273\277|\r\n'
printf 'a\342\200\252|\r\n'

# hangul initial consonant, medial vowel, final consonant
printf '\341\204\200\341\205\241\341\206\250|\r\n'

# these take a cell: soft hyphen, arabic number sign (prepended
# concatenation mark), devanagari sign visarga (spacing mark)
printf 'a\302\255|\r\n'
printf 'a\330\200|\r\n'
printf 'a\340\244\203|\r\n'
