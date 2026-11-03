#!/bin/sh
# Triage what a tyfuzz campaign saved, into fix commits.
#
#   triage.sh <findings dir> <one-shot ASAN tyfuzz> [work dir]
#
# The findings dir is afl-fuzz's -o, holding a crashes/ and a hangs/ per
# instance, and the ASAN binary is the one-shot build of src/bin/tyfuzz
# (build-afl-asan). One-shot, not persistent: a crash that only reproduces
# while sharing state with previous inputs is not one we can fix, and the
# campaign runs with AFL_PERSISTENT_RECORD=100 so those inputs are saved
# alongside it.
#
# For every input:
# 1. replay it through the one-shot ASAN build;
# 2. a crash that does not reproduce is dropped;
# 3. a hang is replayed twice more, at repeat exponent k (byte 1) and k+1:
#    a replay whose time more than doubles is a quadratic re-parse and goes
#    to the fix list, one whose time merely grows with the input is a big
#    but legitimate 4 MiB stream, not a bug;
# 4. what reproduces is bucketed by the top of the ASAN stack, and the
#    first input of each bucket is minimized with afl-tmin.
#
# The buckets and minimized inputs land in the work dir. The fix commit for
# each bucket is a hand-written tests/<name>.sh reproducer, never the crash
# file itself.

set -eu

findings=${1:?usage: triage.sh <findings dir> <one-shot ASAN tyfuzz> [work dir]}
asan=${2:?usage: triage.sh <findings dir> <one-shot ASAN tyfuzz> [work dir]}
work=${3:-triage}

mkdir -p "$work"
: > "$work/buckets.txt"
: > "$work/hang-times.txt"

# $1: the input file; the replay appends its log under $work, and what is
# echoed is the wall clock time of the replay, in whole seconds.
replay_time()
{
    t0=$(date +%s)
    timeout -s ABRT 60 "$asan" "$1" > "$work/replay.log" 2>&1 || true
    t1=$(date +%s)
    echo $((t1 - t0))
}

# $1: the input file, $2: the exponent to write over byte 1
copy_with_exp()
{
    cp "$1" "$work/replay-input"
    printf "\\$(printf '%03o' "$2")" \
        | dd of="$work/replay-input" bs=1 seek=1 count=1 conv=notrunc \
        2>/dev/null
}

for f in "$findings"/*/crashes/id:* "$findings"/*/hangs/id:*; do
    [ -f "$f" ] || continue
    kind=$(basename "$(dirname "$f")")
    name="$kind/$(basename "$f")"

    k=$(od -An -tu1 -j1 -N1 "$f" | tr -d ' ')

    if [ "$kind" = hangs ]; then
        copy_with_exp "$f" "$k"
        t_k=$(replay_time "$work/replay-input")
        copy_with_exp "$f" $(( (k + 1) % 256 ))
        t_k1=$(replay_time "$work/replay-input")
        printf '%s k=%ds k+1=%ds\n' "$name" "$t_k" "$t_k1" \
            >> "$work/hang-times.txt"
        # second resolution: 1 -> 2 is noise, 4 -> 20 is a re-parse
        if [ "$t_k1" -le $(( 2 * (t_k + 1) )) ]; then
            continue
        fi
    elif replay "$f" > /dev/null 2>&1; then
        echo "$name: does not reproduce, dropped" >&2
        continue
    fi

    # bucket by the top of the ASAN stack
    bucket=$(head -100 "$work/replay.log" |
        grep -A3 'ERROR: AddressSanitizer' |
        tail -3 | cksum | cut -d' ' -f1)
    echo "$bucket $f" >> "$work/buckets.txt"
    if [ "$(grep -c "^$bucket " "$work/buckets.txt")" -eq 1 ]; then
        afl-tmin -i "$f" -o "$work/min-$bucket" \
            -- "$asan" "@@" > /dev/null 2>&1 || true
    fi
done

echo "buckets (one minimized input each):" >&2
sort "$work/buckets.txt" |
    awk '{ if ($1 != last) print "  " $2; last = $1 }' >&2
echo "hang replay times: $work/hang-times.txt" >&2
