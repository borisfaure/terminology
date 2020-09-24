#!/bin/sh
set -e
set -u

COMPRESS=1

EET=$1
shift
OUTPUT=$1
shift
JSON2DESC=$(dirname "$0")/json2desc.py

# work on a temporary file till every insertion worked
TMP_EET=$(mktemp "$OUTPUT-XXXXXX")
# trap to avoid creating orphan files
trap 'rm -f "$TMPFILE"' INT TERM HUP EXIT

for JSON in "$@"
do
   # use the name, without extension as key in eet
   KEY=$(basename "$JSON" ".desc")
   DESC="${KEY}.desc"
   $JSON2DESC "$JSON" "$DESC"
   $EET -e "$TMP_EET" "$KEY" "$DESC" "$COMPRESS"
done

# atomic rename to the expected output file
mv "$TMP_EET" "$OUTPUT"

# file successfully renamed, so need to trap to rename temp file
trap - INT TERM HUP EXIT
