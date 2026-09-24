# AGENTS.md

Guidance for AI coding agents, and for the people driving them, who
contribute to Terminology. The aim is simple: a contribution made with an AI
tool must be as easy to review as a careful human one.

The human submitting the change is accountable for every line of it. If you
cannot explain why a line is there, it does not belong in the patch.

## Project overview

Terminology is a terminal emulator written in C on top of the EFL
(Enlightenment Foundation Libraries). The code lives in `src/bin/`;
[DESIGN.md](DESIGN.md) maps each file to its role. The hot spots are:

* `termptyesc.c`: escape sequence parsing
* `termpty.c`, `termptyops.c`: terminal state and history
* `termio.c`, `termiointernals.c`: the terminal widget and its rendering

Terminal behaviour is defined by outside references, not by this code. When
you implement or change a control sequence, follow the source and cite it:
[ECMA-48](https://ecma-international.org/publications-and-standards/standards/ecma-48/),
[XTerm Control Sequences](https://invisible-island.net/xterm/ctlseqs/ctlseqs.html),
the DEC VT manuals, or the specification of the protocol in question.

## Build and test

```sh
meson setup -Dtests=true build
ninja -C build
meson test -C build --print-errorlogs
```

`meson test` runs:

* `escape-codes*`: every `tests/*.sh` script is piped through `tytest`, which
  outputs an md5 checksum of the terminal state. The checksum is compared to
  `tests/tests.results`. The same scripts are replayed with 1, 3 and 7 byte
  reads, and again with the SIMD kernels disabled. All runs must agree.
* `unit` / `unit-scalar`: the C unit tests compiled into `tytest`.

CI also builds with sanitizers and runs coccinelle
(`scripts/coccinelle/coccicheck.sh`). Run it if you have coccinelle
installed.

## Tests are mandatory

Before you claim a change is done, **build it and run the full test suite.
Every test must pass.** Report failures as they are. Do not hide them, skip
them, or weaken a test to make it pass.

Every bug fix comes with a test that fails without the fix. Every new feature
comes with tests that cover it, edge cases included. What a regression test
is for is making sure the same bug never comes back.

### Escape code tests

1. Add `tests/<name>.sh`: a POSIX shell script that `printf`s the sequences
   under test. Name it after the sequence or the bug (`cup.sh`,
   `crash_empty_osc.sh`). Cover the edge cases: default and missing
   parameters, out of range values, margins, sequences split across reads.
2. Rebuild, then compute its checksum with
   `bash tests/<name>.sh | build/src/bin/tytest`
   and add a `<name>.sh <checksum>` line to `tests/tests.results`.
3. Check the checksum means what you think it does: run the script in
   Terminology and look at the result.

If your change modifies the checksum of an existing test, that is a
behaviour change. Look at why. Update the line only if the new behaviour is
the correct one, and say why in the commit message. Never regenerate the
whole `tests.results` file to make the suite pass.

### Unit tests

Logic that can be checked without a terminal (parsers, helpers, data
structures) gets a C unit test:

* declare `int tytest_<name>(void);` in `src/bin/unit_tests.h`
* register it in the table in `src/bin/tytest.c`
* write it next to the code it tests, inside `#if defined(BINARY_TYTEST)`

Run a single one with `build/src/bin/tytest <name>`.

## Performance changes need numbers

A change made for performance must prove it. Either an existing benchmark
shows the improvement, or the change adds a benchmark that does. "It should
be faster" is not enough.

```sh
meson configure build -Dbenchmarks=true
ninja -C build
meson test -C build --benchmark -v
```

`tybench` measures the pty intake path (UTF-8 decoding and
`termpty_handle_buf()`) on generated corpora (`tests/bench/gen_corpus.py`).
If the code you optimize is not covered, extend the corpus or add a
benchmark that exercises it.

* Measure before and after on the same machine and build type, with enough
  iterations to rise above the noise.
* Put the before and after numbers in the commit message, with the
  benchmark and corpus used.
* Show that no other workload got slower, and that the tests still pass: a
  faster wrong answer is a regression.
* Complexity that only buys a gain within the noise will be rejected.

## Code that explains itself

The code is the *how*. It must be readable on its own, through the names of
its functions, types and variables and through its structure, not through
comments that paraphrase it.

### Naming

The further a name travels from its declaration, the more it has to say.

* A local used over a few lines can be short: `i`, `x`, `it`.
* A static function, or a variable that spans a long function, needs a
  descriptive name: `_cursor_to_start_of_line()`, `_tab_forward()`.
* Anything visible from another file (functions, types, struct fields,
  globals) must be meaningful without reading its implementation, and
  carries its module prefix: `termpty_`, `termio_`, `Termpty`, `Termcell`.

Short names follow the conventions already used everywhere in the code:

| Name         | Meaning                                                   |
|--------------|-----------------------------------------------------------|
| `i`, `j`, `k`| numeric loop counters                                     |
| `it`         | an `Eina_Iterator`                                        |
| `l`          | an `Eina_List` node while walking a list                  |
| `x`, `y`     | a position                                                |
| `w`, `h`     | a width and a height                                      |
| `xx`, `yy`, `ww`, `hh` | a second position or size in the same scope     |
| `cx`, `cy`   | a cursor position                                         |
| `ty`         | the `Termpty *` being worked on                           |
| `sd`         | the private data of a smart object                        |

When several objects of the same kind are in scope, name them by their role
(`src`, `dst`, `old_w`), not `a`, `b`, `tmp2`.

### Comments

Write comments for:

* **A function's contract**, when its name and signature are not enough:
  what it expects, what it returns, who owns what.
* **Quirks that need outside knowledge**: a bug in another program being
  worked around, a spec corner case, a compatibility choice, a
  non-obvious performance trade-off. Link to the source material whenever
  one exists, as in `/* from https://www.compuphase.com/cmetric.htm */`.

Do not write comments that:

* restate what the next line does (`/* increment i */`)
* narrate the change (`/* now uses the new helper */`, `/* fixed */`)
  — that belongs in the commit message
* section off trivial code, or add banners, or document every parameter
  of a static helper whose names already say it all

If code needs a comment to be understood, first try a better name, a smaller
function or a clearer structure.

### Style

* EFL style: 3 space indentation, braces on their own line indented by 2,
  the function's return type on its own line. Look at the surrounding code
  and match it exactly.
* Use EFL facilities (`Eina_Bool`, `Eina_List`, `EINA_SAFETY_*`, `ERR()`,
  `DBG()`) rather than reinventing them.
* Keep the diff minimal: no drive-by reformatting, reordering or renaming of
  code you did not otherwise need to touch. A cleanup is its own commit.
* This is a hot path program: think about allocations and per-cell or
  per-character work in the parser and the renderer.

## Commits

One commit is one logical change. It builds and passes the tests on its own
so that `git bisect` keeps working. A test may come in its own commit, as in
`tests: add apc.sh to check APC sequences are swallowed`.

### Subject

`<component>: <what changed>`, lowercase, imperative mood, no trailing
period, around 70 characters at most. The component is usually the file
or area touched without its extension:

```
termptyesc: ignore APC escapes
termio: render the overline attribute
tests: add sgr-long.sh to test {over/under}line
data/terminfo: advertise Swd and fsl
```

### Body: the *why*, not the *how*

The diff already shows how. The body explains what the reader cannot get
from it:

* why the change is needed: the bug, its symptom, who hits it
* why this approach over the obvious alternatives
* the spec or reference that decides the behaviour, with a link
* anything surprising a reviewer should know: a changed test checksum, a
  corner case that is deliberately left out

Do not list the functions you edited, walk through the diff, or paste test
output. Wrap at 72 columns. A trivial change may have no body at all.

Good:

```
termptyesc: ignore APC escapes

Parse and discard APC control functions (defined in ECMA-48) as XTerm
does, instead of bleeding them through the output.

This is useful if a program wishes to detect support for Kitty images
through dynamic terminal querying; terminals which do not support Kitty
images are expected to ignore such APC sequences.
```

Bad:

```
termptyesc: add APC support

- Added _handle_esc_apc() function
- Modified _handle_esc() to call it
- Added new test
- All tests pass
```

### AI disclosure

Commits written with the help of an AI tool say so with a trailer naming the
tool, for example:

```
Co-Authored-By: <Tool and model name> <noreply@example.com>
```

## Pull requests and reviews

* Keep a pull request focused on one topic. Large or design changing work
  should be discussed in an issue first.
* In the description, explain the problem and how the change was tested.
* Answer review comments yourself, in your own words. Reviewers are
  discussing with you, not with your tool. Do not just re-run the prompt
  and push whatever comes out.

## Do not

* submit code you have not built and tested
* claim tests pass without running them
* add dependencies without discussing it first
* touch generated files by hand (`src/bin/termptydbl.[ch]` are generated,
  see [DEV.md](DEV.md))
* commit build artifacts, profiling output or screenshots
