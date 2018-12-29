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
NB_TESTS=0
OK_TESTS=0
FAILED_TESTS=0

die()
{
    echo "$*" 1>&2
    exit 1
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
  -e, --exitonfailure      Exit as soon as a test fails

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
        -e|-exitonfailure|--exitonfailure)
            EXIT_ON_FAILURE=1
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
   EXIT_ON_FAILURE=$EXIT_ON_FAILURE

EOF
fi

while read -r TEST EXPECTED_CHECKSUM; do
    NB_TESTS=$((NB_TESTS + 1))
    if [ $VERBOSE -ne 0 ]; then
        printf "%s... " "$TEST"
    fi
    TEST_CHECKSUM=$("$TESTDIR"/"$TEST" | "$TYTEST")
    if [ $DEBUG -ne 0 ]; then
        printf "(got %s, expected %s) " "$TEST_CHECKSUM" "$EXPECTED_CHECKSUM"
    fi
    if [ $GENRESULTS -ne 0 ]; then
        printf "%s %s\n" "$TEST" "$TEST_CHECKSUM"
    else
       if [ "$TEST_CHECKSUM" = "$EXPECTED_CHECKSUM" ]; then
          ok "$TEST"
       else
          failed "$TEST"
       fi
    fi
done < "$RESULTS"
summary
