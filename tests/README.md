Very small test framework for Terminology

Goal
----

Avoid regressions in Terminology's code that parse and interprets escape
codes.

Dependencies
------------

- `tytest`, compiled with `-D tests=true` when running `meson`
- a posix shell


Tytest
------

`tytest` is a simple binary that takes escape codes in input and output an md5
sum. This checksum is computed on the state of terminology after parsing and
interpreting those escape codes.


Test cases
----------

Test cases are simple shell scripts that output escape codes.

How to run the tests
--------------------

A shell script called `run_tests.sh` is provided to run all the tests given in
a test results file.
See `run_tests.sh --help` for more information.

Test results file
-----------------

When running a test through `tytest`, an md5 sum is computed. This checksum is
stored with the name of the test in a file called `tests.results`.
If terminology's behaviour changed, then the checksum will change. This will
be noticed by `run_tests.sh` and will show those tests as failed.
