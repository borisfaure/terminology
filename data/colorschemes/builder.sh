#!/bin/sh
set -e
set -u

COMPRESS=1

EET=$1
shift
OUTPUT=$1
shift
INI2DESC=$(dirname "$0")/ini2desc.py

# work on a temporary file till every insertion worked
TMP_EET=$(mktemp "$OUTPUT-XXXXXX")
# trap to avoid creating orphan files
trap 'rm -f "$TMP_EET"' INT TERM HUP EXIT

for INI in "$@"
do
   # use the name, without extension as key in eet
   KEY=$(basename "$INI" ".ini")
   DESC="${KEY}.desc"
   $INI2DESC "$INI" "$DESC"
   $EET -e "$TMP_EET" "$KEY" "$DESC" "$COMPRESS"
done

# atomic rename to the expected output file
mv "$TMP_EET" "$OUTPUT"

# file successfully renamed, so need to trap to rename temp file
trap - INT TERM HUP EXIT
