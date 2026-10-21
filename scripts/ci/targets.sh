#!/bin/sh
# targets.sh --json      the target names, as a workflow matrix
# targets.sh <target>    the target's EFL ref, then its expected features
set -eu

targets() { grep -v -e '^#' -e '^$' "$(dirname "$0")/targets"; }

if [ "${1:-}" = --json ]; then
   sep=
   printf '{"target":['
   targets | while read -r name _; do
      printf '%s"%s"' "$sep" "$name"
      sep=,
   done
   printf ']}\n'
   exit 0
fi

line=$(targets | grep "^${1:?target}[[:space:]]") || {
   echo "unknown target: $1" >&2
   exit 2
}
# shellcheck disable=SC2086
set -- $line
shift
echo "$*"
