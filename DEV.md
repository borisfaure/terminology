# Double Width

The code to decide whether a unicode codepoint should be rendered double-width
is generated from the Unicode specification.
This is done by using `tools/unicode_dbl_width.py`.

1. Download <https://www.unicode.org/Public/UCD/latest/ucdxml/ucd.all.flat.zip>
2. Extract it
3. Run `tools/unicode_dbl_width.py ucd.all.flat.xml src/bin/termptydbl.h src/bin/termptydbl.c`_
4. Commit the files modified

# Terminfo capabilities

The table that answers XTGETTCAP queries is generated from the shipped
terminfo entry, so that the two cannot drift apart.

After editing `data/terminfo/terminology.terminfo`, run
`tools/terminfo_tcap.py data/terminfo/terminology.terminfo src/bin/termptytcap.h`
and commit both files.

# Headless runs

Terminology can run without a display server when Elementary renders
off-screen. It never picks a rendering engine itself: Elementary takes it
from the environment (the engines are listed in `elm_win.h`). In a
container or in CI, the shot engine renders in memory and writes a PNG:

```sh
ELM_ENGINE="shot:delay=2.0:file=/tmp/ty.png" \
    ./build/src/bin/terminology --no-wizard=true -g 80x10 -e ./demo.sh
```

The image is written once the delay expires. `-e` runs the command and the
window closes when it exits, so the command must keep running past the
shot delay (end `demo.sh` with a `sleep`). `--no-wizard=true` (the option
takes a value) skips the first-run configuration wizard.

In a bare container fontconfig may lack its default configuration; point
`FONTCONFIG_FILE` at a minimal one (`<dir>` for the system fonts, a
writable `<cachedir>`).

The shot engine can only be chosen through `ELM_ENGINE`. `ELM_DISPLAY`
names a display system (`x11`, `wl`, `fb`, `buffer`) rather than a
renderer; `ELM_DISPLAY=buffer` renders off-screen but writes no image.
Elementary ignores `ECORE_EVAS_ENGINE`.
