#!/bin/sh
# Generate the raw seeds for the escape sequence parser campaign.
#
#   make-seeds.sh [raw-dir] [tyfuzz]
#   afl-cmin -i "$raw-dir" -o seeds -- "$tyfuzz" @@
#
# One seed per tests/*.sh replay, prefixed with the fuzzers' 3-byte test
# case header: byte 0x07 is 4096-byte reads, bytes 0x00 0x00 mean no
# repeat, so a seed is then exactly what the test fed. The buffering seeds
# below are hand-made on top: unterminated OSC, DCS, APC and CSI with the
# repeat exponent (byte 1) raised so that a few bytes expand into a
# megabyte, the span (byte 2) starting before and after the introducer.

set -eu

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tests_dir=$(dirname "$here")
raw=${1:-"$here/seeds-raw"}
tyfuzz=${2:-"$tests_dir/../build-afl/src/bin/tyfuzz"}

mkdir -p "$raw"

for script in "$tests_dir"/*.sh; do
    name=$(basename "$script")
    out="$raw/$name"
    printf '\x07\x00\x00' > "$out"
    sh "$script" >> "$out" 2>/dev/null || true
    # afl-fuzz works best on small inputs, and afl-cmin keeps the replay
    # cost of the corpus down: drop what is already over 64 KB, a later
    # campaign instance covers what the tail of a benchmark corpus would.
    if [ "$(wc -c < "$out")" -gt 65536 ]; then
        rm "$out"
        echo "dropping $name: over 64 KB" >&2
    fi
done

# Unterminated introducers, so the sequences sit in ty->buf until the
# parser gives up on them. Byte 1 is the exponent written raw: 2^(k mod 21)
# repetitions of the span, capped at 1 MiB by the harness. Byte 0x00 puts
# the span at the introducer; 0x02, right after it.
for k in 10 12 14 16 18 20; do
    oct=$(printf '%03o' "$k")
    i=0
    for seq in '\x1b]0;' '\x1bP0;1|' '\x1b_AAAA' '\x1b[38;2;'; do
        i=$((i + 1))
        for b2 in 0x00 0x02; do
            raw2=$(printf '%s' "$b2" | sed 's/^0x//')
            {
                printf '\x07'
                printf "\\$oct"
                printf "\\x$raw2"
                printf '%b' "$seq"
                printf 'AAAAAAAA'
            } > "$raw/buffering-k$k-$i-$raw2"
        done
    done
done

# Reflow: a backlog of wrapped lines, then resizes narrower, wider, tiny
# and back, so the rewrap of the screen and of the backlog both run.
resizes='\033[8;24;40t\033[8;10;133t\033[8;3;7t\033[8;24;80t'
{
    printf '\x07\x00\x00'
    for i in $(seq 60); do
        printf 'line %03d ' "$i"
        printf 'abcdefghij%.0s' $(seq 15)
        printf '\r\n'
    done
    printf "$resizes"
} > "$raw/reflow-wrapped"
{
    printf '\x07\x00\x00'
    for i in $(seq 40); do
        printf '%03d ' "$i"
        printf '中文字符e\xcc\x81%.0s' $(seq 12)
        printf '\r\n'
    done
    printf '\033[8;24;41t\033[8;24;3t\033[8;24;80t'
} > "$raw/reflow-wide"
{
    printf '\x07\x00\x00'
    printf 'endless%.0s' $(seq 600)
    printf "$resizes"
} > "$raw/reflow-endless"
{
    printf '\x07\x00\x00'
    for i in $(seq 30); do
        printf 'main %03d ' "$i"
        printf 'abcdefghij%.0s' $(seq 12)
        printf '\r\n'
    done
    printf '\033[?1049h\033[2;20r\033[10;10Halt screen'
    printf "$resizes"
    printf '\033[?1049l'
    printf "$resizes"
} > "$raw/reflow-altscreen"

echo "$raw:" >&2
echo "  afl-cmin -i '$raw' -o '${raw%seeds-raw}seeds' -- '$tyfuzz' @@" >&2
