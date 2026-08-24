#!/bin/sh
set -e
set -u

TYTEST="../build/src/bin/tytest"
RESULTS="tests.results"
TESTDIR="."
VERBOSE=0
DEBUG=0
GENRESULTS=0
EXIT_ON_FAILURE=0
CHUNK=""
TEST_SHELL=""
NB_TESTS=0
OK_TESTS=0
FAILED_TESTS=0

die()
{
    echo "$*" 1>&2
    exit 1
}

# The test scripts write their escape sequences as \xNN, which POSIX does not
# require printf to understand: dash, /bin/sh on Debian and Ubuntu, emits them
# verbatim instead. So the scripts cannot simply be run through /bin/sh; pick a
# shell whose printf does the right thing. meson passes one it found at
# configure time, this is for standalone runs.
detect_shell()
{
    for CANDIDATE in "$@"; do
        if [ "$("$CANDIDATE" -c 'printf "\x41"' 2>/dev/null)" = "A" ]; then
            printf '%s' "$CANDIDATE"
            return 0
        fi
    done
    return 1
}
ESC="\033"
GREEN="${ESC}[32m"
BOLD_RED="${ESC}[31;1m"
RESET_COLOR="${ESC}[0m"

ok()
{
    TEST=$1
    OK_TESTS=$((OK_TESTS + 1))
    if [ $VERBOSE -ne 0 ]; then
        printf "${GREEN}✔${RESET_COLOR}\n"
    fi
}

failed()
{
    TEST=$1
    FAILED_TESTS=$((FAILED_TESTS + 1))
    if [ $VERBOSE -ne 0 ]; then
        printf "${BOLD_RED}✘${RESET_COLOR}\n"
    fi
    if [ $EXIT_ON_FAILURE -ne 0 ]; then
        exit 1
    fi
}

summary()
{
    if [ $VERBOSE -ne 0 ]; then
        if [ $FAILED_TESTS -ne 0 ]; then
            printf "$BOLD_RED=== $OK_TESTS/$NB_TESTS tests passed, $FAILED_TESTS tests failed ===$RESET_COLOR\n"
        else
            printf "$GREEN=== $OK_TESTS/$NB_TESTS tests passed ===$RESET_COLOR\n"
        fi
    fi

    if [ $FAILED_TESTS -ne 0 ]; then
        exit 1
    fi
}

show_help()
{
    cat <<HELP_EOF
Usage:

   $0 [options]

where options are:

  -t, --tytest=PATH        Path to the tytest binary
  -r, --results=PATH       Path to the result file
  -d, --testdir=PATH       Path to the test files
  -s, --shell=PATH         Shell used to run the test scripts. Defaults to the
                           first of sh, bash, ksh, zsh whose printf understands
                           \xNN escapes.
  -e, --exitonfailure      Exit as soon as a test fails
  -c, --chunk=N            Feed tytest N bytes per read, to exercise sequences
                           split across read boundaries. Results must match the
                           default run exactly.

Misc options:
  -v, --verbose            Be verbose about what is being done
  --debug                  Debug tests
  --genresults             Output a results file
  -h, --help               Show this help.
HELP_EOF
}

while [ $# -gt 0 ]; do
    arg=$1
    shift
    option=$(echo "'$arg'" | cut -d'=' -f1 | tr -d "'")
    value=$(echo "'$arg'" | cut -d'=' -f2- | tr -d "'")
    if [ x"$value" = x"$option" ]; then
        value=""
    fi

    case $option in
        -h|-help|--help)
            show_help
            exit 0
            ;;
        -v|-verbose|--verbose)
            VERBOSE=1
            ;;
        -debug|--debug)
            DEBUG=1
            ;;
        -genresults|--genresults)
           GENRESULTS=1
           ;;
        -t|-tytest|--tytest)
            if [ -z "$value" ]; then
                value=$1
                shift
            fi
            TYTEST=$value
            ;;
        -r|-results|--results)
            if [ -z "$value" ]; then
                value=$1
                shift
            fi
            RESULTS=$value
            ;;
        -d|-testdir|--testdir)
            if [ -z "$value" ]; then
                value=$1
                shift
            fi
            TESTDIR=$value
            ;;
        -s|-shell|--shell)
            if [ -z "$value" ]; then
                value=$1
                shift
            fi
            TEST_SHELL=$value
            ;;
        -e|-exitonfailure|--exitonfailure)
            EXIT_ON_FAILURE=1
            ;;
        -c|-chunk|--chunk)
            if [ -z "$value" ]; then
                value=$1
                shift
            fi
            CHUNK="--chunk=$value"
            ;;
        *)
            echo "Unknown option: $option" 1>&2
            ;;
    esac
done

if [ ! -x "$TYTEST" ]; then
    die "Invalid tytest binary file: $TYTEST"
fi
if [ ! -r "$RESULTS" ]; then
    die "Invalid results file: $RESULTS"
fi
if [ ! -d "$TESTDIR" ]; then
    die "Invalid test directory: $TESTDIR"
fi
if [ -z "$TEST_SHELL" ]; then
    TEST_SHELL=$(detect_shell sh bash ksh zsh) ||
        die "No shell found whose printf understands \\xNN escapes"
fi
if [ $GENRESULTS -ne 0 ]; then
   DEBUG=0
   VERBOSE=0
fi


if [ $DEBUG -ne 0 ]; then
    cat <<EOF
Using:

   TYTEST=$TYTEST
   RESULTS=$RESULTS
   TESTDIR=$TESTDIR
   TEST_SHELL=$TEST_SHELL
   EXIT_ON_FAILURE=$EXIT_ON_FAILURE

EOF
fi

while read -r TEST EXPECTED_CHECKSUMS; do
    if case "${TEST}" in \#*) false;; esac; then
        NB_TESTS=$((NB_TESTS + 1))
        if [ $VERBOSE -ne 0 ]; then
            printf "%s... " "$TEST"
        fi
        if [ -n "$CHUNK" ]; then
            TEST_CHECKSUM=$("$TEST_SHELL" "$TESTDIR"/"$TEST" | "$TYTEST" "$CHUNK")
        else
            TEST_CHECKSUM=$("$TEST_SHELL" "$TESTDIR"/"$TEST" | "$TYTEST")
        fi
        if [ $DEBUG -ne 0 ]; then
            printf "(got %s, expected %s) " "$TEST_CHECKSUM" "$EXPECTED_CHECKSUMS"
        fi
        if [ $GENRESULTS -ne 0 ]; then
            printf "%s %s\n" "$TEST" "$TEST_CHECKSUM"
        else
            OK=0
            for CHECKSUM in $EXPECTED_CHECKSUMS; do
                if [ "$TEST_CHECKSUM" = "$CHECKSUM" ]; then
                    OK=1
                    break
                fi
            done
            if [ "$OK" -eq 1 ]; then
                ok "$TEST"
            else
                failed "$TEST"
            fi
        fi
    fi
done < "$RESULTS"
summary
