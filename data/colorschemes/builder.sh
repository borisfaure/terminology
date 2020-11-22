#!/bin/sh
set -e
set -u

EET=$1
shift
OUTPUT=$1
shift
ADD_COLOR_SCHEME=$(dirname "$0")/add_color_scheme.sh

# work on a temporary file till every insertion worked
TMP_EET=$(mktemp "$OUTPUT-XXXXXX")
# trap to avoid creating orphan files
trap 'rm -f "$TMP_EET"' INT TERM HUP EXIT

for INI in "$@"
do
   # use the name, without extension as key in eet
   $ADD_COLOR_SCHEME "$EET" "$TMP_EET" "$INI"
done

# atomic rename to the expected output file
mv "$TMP_EET" "$OUTPUT"

# file successfully renamed, so need to trap to rename temp file
trap - INT TERM HUP EXIT
