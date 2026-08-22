#!/bin/sh
# Push far more content through the screen than the backlog can hold, so that
# the scrollback ring wraps and old rows are evicted.
#
# Most of the suite never scrolls anything off screen, which left the whole
# save/evict path uncovered. It is also exactly the path a bulk text-append
# fast path is most likely to break, so it is worth exercising deliberately:
# long printable runs, runs that wrap at the right margin, rows of differing
# lengths, and attribute changes that must survive being saved and read back.

# Known state.
printf '\033c'

# Backlog is 50 rows in the test harness; 120 lines guarantees eviction.
i=1
while [ $i -le 120 ]; do
    # Vary the colour so saved rows are not all attribute-identical.
    printf '\033[3%d;4%dm' $((i % 8)) $(((i / 8) % 8))

    case $((i % 4)) in
    0)
        # Short line: leaves the tail of the row untouched.
        printf 'row %d\n' $i
        ;;
    1)
        # Long line: overruns 80 columns and must autowrap.
        printf 'row %d ' $i
        j=0
        while [ $j -le 12 ]; do
            printf 'abcdefghi'
            j=$((j + 1))
        done
        printf '\n'
        ;;
    2)
        # Exactly-at-margin content, to catch off-by-one wrapping.
        printf 'row %d ' $i
        printf '%s' 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx'
        printf '\n'
        ;;
    3)
        # Wide characters and a combining mark: the lead cell carries the
        # codepoint with dblwidth set and the trailing cell must follow it into
        # the backlog intact.
        printf 'row %d 漢字テスト e\314\201 done\n' $i
        ;;
    esac
    i=$((i + 1))
done

printf '\033[0m'

# Scroll a little more with explicit SU/SD so the ring is left mid-rotation
# rather than neatly aligned.
printf '\033[5S'
printf '\033[2T'
