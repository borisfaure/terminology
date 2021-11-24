#!/bin/sh
set -e
set -u

COMPRESS=1

EET=$1
shift
EET_FILE=$1
shift
INI=$1
shift

INI2DESC=$(dirname "$0")/ini2desc.py
GET_NAME=$(dirname "$0")/get_name.py

NAME=$($GET_NAME "$INI")

# ensure output directory exists
mkdir -p "$(dirname "$EET_FILE")"

# generate desc on a temporary file
TMP_DESC=$(mktemp "$NAME-DESC-XXXXXX")
TMP_EET=$(mktemp "$NAME-EET-XXXXXX")

# trap to avoid creating orphan files
trap 'rm -f "$TMP_DESC" "$TMP_EET"' INT TERM HUP EXIT

NAME=$($GET_NAME "$INI")

[ ! -w "$EET_FILE" ] && touch "$EET_FILE"

echo "Generating $EET_FILE"

$INI2DESC "$INI" "$TMP_DESC"
$EET -e "$EET_FILE" "$NAME" "$TMP_DESC" "$COMPRESS"

rm "$TMP_DESC"

# file successfully written, so need to trap to rename temp file
trap - INT TERM HUP EXIT
