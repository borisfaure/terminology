#!/bin/sh
set -e
set -u

COMPRESS=1

EET=$1
shift
OUTPUT=$1
shift

# work on a temporary file till every insertion worked
TMP_EET=$(mktemp "$OUTPUT-XXXXXX")
# trap to avoid creating orphan files
trap 'rm -f "$TMPFILE"' INT TERM HUP EXIT

for DESC in "$@"
do
   # use the name, without extension as key in eet
   KEY=$(basename "$DESC" ".desc")
   $EET -e "$TMP_EET" "$KEY" "$DESC" "$COMPRESS"
done

# atomic rename to the expected output file
mv "$TMP_EET" "$OUTPUT"

# file successfully renamed, so need to trap to rename temp file
trap - INT TERM HUP EXIT
