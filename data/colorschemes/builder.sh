#!/bin/bash
set -e
set -u

EET=$1
shift
ADD_COLOR_SCHEME=$(dirname "$0")/add_color_scheme.sh

for INI in "$@"
do
   echo "Building $INI"
   # use the name, without extension as key in eet
   $ADD_COLOR_SCHEME "$EET" "$INI"
done
