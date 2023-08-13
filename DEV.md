# Double Width

The code to decide whether a unicode codepoint should be rendered double-width
is generated from the Unicode specification.
This is done by using `tools/unicode_dbl_width.py`.

1. Download <https://www.unicode.org/Public/UCD/latest/ucdxml/ucd.all.flat.zip>
2. Extract it
3. Run `tools/unicode_dbl_width.py ucd.all.flat.xml src/bin/termptydbl.h src/bin/termptydbl.c`_
4. Commit the files modified
