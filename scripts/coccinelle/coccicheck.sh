#!/bin/sh
set -eu

DIR="src/bin"
COCCI_FILES="
andconst.cocci
badzero.cocci
continue.cocci
free_stack.cocci
mutex.cocci
notand.cocci
notnull.cocci
null_ref.cocci
unused.cocci
use_after_iter.cocci
div_round_up.cocci
"

HAS_ERROR=0
for f in $COCCI_FILES; do
   OPTIONS=""
   if [ "$COCCI_FILES" = "div_round_up" ]; then
      OPTIONS="--defined DIV_ROUND_UP"
   fi
   CMD="spatch --timeout 200 --very-quiet --cocci-file scripts/coccinelle/$f --include-headers --dir $DIR $OPTIONS"
   OUT=$($CMD)
   echo "$CMD"
   if [ -n "$OUT" ]; then
      echo "$OUT"
      HAS_ERROR=1
   fi
done
exit $HAS_ERROR
