Terminology 1.2.0
=================

This is an EFL terminal emulator with some extra bells and whistles.

:warning: :warning: :warning:

> FOR ANY ISSUES, PLEASE CONSULT THEM ON
> http://issues.terminolo.gy/

:warning: :warning: :warning:


Requirements
------------

  * `efl` (>= 1.20.0)
  * `elementary` (>= 1.20.0)

Please see http://www.enlightenment.org for information on these.


Compiling
---------

Once you have met requirements, compiling and installing is simple:

```sh
  meson build
  cd build
  ninja
  ninja install
```

Note: to make terminology work with input methods in general you need:

```sh
  export ECORE_IMF_MODULE="xim"
  export XMODIFIERS="@im=none"
```


Mouse controls
--------------

  * `Right mouse button` = controls menu
  * `Middle mouse button` = paste highlight selection
  * `Left mouse button/drag` = make highlight
  * `Wheel` = scroll up or down in history
  * `Ctrl+Wheel` = zoom font size up/down
  * `Ctrl+Left mouse button/drag` = make block selection highlight


Default key controls
--------------------

  * `Shift+PgUp` = Scroll 1 page up
  * `Shift+PgDn` = Scroll 1 page down
  * `Shift+Up` = Scroll 1 line up
  * `Shift+Down` = Scroll 1 line down
  * `Shift+Home` = Scroll to the top of the backlog
  * `Shift+End` = Reset scroll
  * `Shift+Left` = switch focus to previous terminal inside a window
  * `Shift+Right` = switch focus to next terminal inside a window
  * `Shift+Insert` = Paste Clipboard (`ctrl+v/c`) selection
  * `Shift+Ctrl+Insert` = Paste Primary (highlight) selection
  * `Shift+Keypad-Plus` = Font size up 1
  * `Shift+Keypad-Minus` = Font size down 1
  * `Shift+Keypad-Multiply` = Reset font size
  * `Shift+Keypad-Divide` = Copy highlight to Clipboard (same as `Ctrl+c` in gui apps)
  * `Ctrl+PgUp` = switch focus to previous terminal inside a window
  * `Ctrl+PgDn` = switch focus to next terminal inside a window
  * `Ctrl+Shift+t` = create new terminal on top of current inside window (tabs)
  * `Ctrl+Shift+End` = close the focused terminal.
  * `Ctrl+Shift+h` = toggle displaying the miniview of the history
  * `Ctrl+Shift+Home` = bring up "tab" switcher
  * `Ctrl+Shift+PgUp` = split terminal horizontally (1 term above the other)
  * `Ctrl+Shift+PgDn` = split terminal vertically (1 term to the left of the other)
  * `Ctrl+Shift+c` = copy current selection to clipboard
  * `Ctrl+Shift+v` = paste current clipboard selection
  * `Alt+Home` = Enter command mode (enter commands to control terminology itself)
  * `Alt+Return` = Paste primary selection
  * `Alt+g` = Group input: send input to all visible terminals in the window
  * `Alt+Shift+g` = Group input: send input to all terminals in the window
  * `Alt+w` = Copy selection to primary
  * `Alt+Up` = Focus the terminal above
  * `Alt+Down` = Focus the terminal below
  * `Alt+Left` = Focus the terminal on the left
  * `Alt+Right` = Focus the terminal on the right
  * `Ctrl+Alt+Equal` = Font size up 1
  * `Ctrl+Alt+Minus` = Font size down 1
  * `Ctrl+Alt+0` = Reset font size
  * `Ctrl+Alt+9` = Big font size
  * `Ctrl+Alt+t` = Set terminal title
  * `Ctrl+1` = switch to terminal tab 1
  * `Ctrl+2` = switch to terminal tab 2
  * `Ctrl+3` = switch to terminal tab 3
  * `Ctrl+4` = switch to terminal tab 4
  * `Ctrl+5` = switch to terminal tab 5
  * `Ctrl+6` = switch to terminal tab 6
  * `Ctrl+7` = switch to terminal tab 7
  * `Ctrl+8` = switch to terminal tab 8
  * `Ctrl+9` = switch to terminal tab 9
  * `Ctrl+0` = switch to terminal tab 10



Themes
------

Apart from the ones shipped with Terminology, themes can be stored in
`~/.config/terminology/themes/` .


Companion tools
---------------

Terminology ships with a set of tools to help you get the best out of
Terminology.

  * `tyls`: list directory contents with bells and whistles
  * `tyalpha`: set transparency level of the background
  * `tybg`: change the background image
  * `tycat`: display inline a media file or a URI
  * `typop`: display in a popup a media file or a URI
  * `tyq`: queue media files or URI to be popped up
  * `tysend`: send files to the terminal (useful through ssh)



Extended escapes for terminology only
--------------------------------------

`[\033][}][COMMAND][\000]`
i.e.
  1. `ESC` char (`\033` or `0x1b`)
  2. `}` char
  3. a sequence of UTF8 chars other than `nul` (`\000` or `0x00`).
  4. `\000` char (`nul` byte or `0x00` to indicate end of sequence)

e.g.
  `echo -n '\033}Hello world\000'`

## Commands

In the following, any values inside square brackets `[]` are to be replaced
by some content (numbers, strings, paths, url's etc.).

examples:

  * `aa[PATH-OF-FILE]` should become something like: `aa/tmp/file.png`
  * `aa[true/false]` should become something like:  `aatrue` or `aafalse`

## Available commands

  * `pn[FULL-PATH-OR-URL]`
    popup the given media file/url now

  * `pq[FULL-PATH-OR-URL]`
     queue a popup for the given media file/url

  * `bt[FULL-PATH-OR-URL]`
    set the terminal background media file/url temporarily

  * `bp[FULL-PATH-OR-URL]`
    set the terminal background media file/url permanently

  * `at[on/true/yes/off/false/no]`
    set the terminal alpha state to be on, or off temporarily

  * `ap[on/true/yes/off/false/no]`
    set the terminal alpha state to be on, or off permanently

  * `qs`
  query grid and font size. stdin will have written to it:
    `W;H;FW;FH\n`
      * where `W` is the width of the terminal grid in characters
      * where `H` is the height of the terminal grid in characters
      * where `FW` is the width of 1 character cell in pixels
      * where `FH` is the height of 1 character cell in pixels

  * `is[CW;H;FULL-PATH-OR-URL]`
    insert _Stretched_ media (where image will stretch to fill the cell area)
    and define expected cell area to be `W` cells wide and `H` cells high,
    with the image/media/url.
      * where `C` is the replace character to be identified in later text
      * where `W` is the width in character cells (up to 511).
      * where `H` is the height in character cells (up to 511).

    note that this escape alone has no effect. It indicates a future
    intention of inserting media into the terminal. the terminal will
    EXPECT a grid of `WxH` _replace characters_ to follow, with each
    sequence of such replace characters beginning with a `ib` escape command
    and ending with an 'ie' escape command.

    the `FULL-PATH-OR-URL` for all  the `i` commands (`is`, `ic`, `if`, `it`)
    may be of the form:
      * `/full/path/to/file.png`
      * OR `/full/path/to/link\n/full/path/to/file.png`
    where a newline character separates a URI for a link and a full
    path to a file to display in the region. the link is the
    destination URI when a user may click on the given media image.

    example:
    ```sh
      printf("\033}is#5;3;%s\000"
             "\033}ib\000#####\033}ie\000\n"
             "\033}ib\000#####\033}ie\000\n"
             "\033}ib\000#####\033}ie\000\n", "/tmp/icon.png");
    ```
    note that `#` is the replace character, and later `#` chars if inside
    begin/end escapes, will be replaced by the given media indicated
    in the insert media escape.

  * `ic[CW;H;FULL-PATH-OR-URL]`
    insert _Centered_ media (centered in cell area). Otherwise parameters are
    identical to the `is` command, but retains aspect and is padded by
    blank space.

  * `if[CW;H;FULL-PATH-OR-URL]`
    insert _Filled_ media (fill in cell area). Otherwise parameters are
    identical to the `is` command but ensures the entire area is
    filled like a background even if media goes beyond cell bounds and is
    clipped.

  * `it[CW;H;FULL-PATH-OR-URL]`
    insert _Thumb_ media (thumbnail cell area). Otherwise parameters are
    identical to the `is` command, but uses thumbnail generation to
    make a fast to load but low resolution version (cached) of the media.

  * `it[CW;H;LINK\nFULL-PATH-OR-URL]`
    insert _Thumb_ media with link (thumbnail cell area). Otherwise parameters
    are identical to the `is` command, but uses thumbnail generation to
    make a fast to load but low resolution version (cached) of the media.
    `LINK` is a path (or url) to open _when_ the thumb is clicked on by the
    user.

  * `ij[CW;H;FULL-PATH\nGROUP][\n][cmd1][\r\n][cmd2]...`
    insert _Edje_ object with file path given, and the group name given.
    The command list (and the `\n` whitespace delimiter before the list)
    is optional, but is executed/parsed before display of the actual
    object to do things like initialize its initial state. See `iC`
    command for more details on the command list.

    Be aware that once you have an object with an assigned channel id,
    it can send back signals and messages via escapes on stdin. These
    escapes will follow terminology extended escape format
    `(ESC + } ... \000)`, and the content will be one of:

      * `;CHID\ndrag\nPART NAME\nVAL1\nVAL2`
      * `;CHID\ndrag,start\nPART NAME\nVAL1\nVAL2`
      * `;CHID\ndrag,stop\nPART NAME\nVAL1\nVAL2`
      * `;CHID\ndrag,step\nPART NAME\nVAL1\nVAL2`
      * `;CHID\drag,set\nPART NAME\nVAL1\nVAL2`
      * `signal;CHID\nSIGNAL STRING\nSOURCE STRING`
      * `message;CHID\nOPCODE ID\nstring\nSTRING1`
      * `message;CHID\nOPCODE ID\nint\nINT1`
      * `message;CHID\nOPCODE ID\nfloat\nFLOAT1`
      * `message;CHID\nOPCODE ID\nstring_set\nSTRING1\nSTRING2\nSTRING3...`
      * `message;CHID\nOPCODE ID\nint_set\nINT1\nINT2\nINT3...`
      * `message;CHID\nOPCODE ID\nfloat_set\nFLOAT1\nFLOAT2\nFLOAT3...`
      * `message;CHID\nOPCODE ID\nstring_int\nSTRING1\nINT1`
      * `message;CHID\nOPCODE ID\nstring_float\nSTRING1\nFLOAT1`
      * `message;CHID\nOPCODE ID\nstring_int_set\nSTRING1\nINT1\nINT2\nINT3...`
      * `message;CHID\nOPCODE ID\nstring_float_set\nSTRING1\nFLOAT1\nFLOAT2\nFLOAT3...`

  * `iC[CHID]\n[cmd1][\r\n][cmd2][\r\n][cmd3]...`
    send a _Command_ (`cmd1`, `cmd2`, `cmd3` etc.) to an existing named
    channel id `CHID`. this can be a list of 1 or more command strings,
    with each string broken by a continuous sequence of 1 or more `\r`
    and/or `\n` characters. e.g. `\n`, `\r`, `\r\n`, `\n\r`,
    `\n\r\n\n\n`, etc.

    commands understood are:

      * `text\nPART NAME\nTEXT STRING`
        set the text of `PART NAME` to string `TEXT STRING`
      * `emit\nSIGNAL STRING\nSOURCE STRING`
        emit signal `SIGNAL STRING` `SOURCE STRING`
      * `drag\nPART NAME\n[value|size|step|page]\nVAL1\nVAL2`
        manipulate dragable of name `PART NAME` and either set `value`,
        `size`, `step` or `page` properties to `VAL1` and `VAL2` for `x`
        and `y`, where `VAL1` and `VAL2` are floating point values
      * `chid\nCHANNEL ID`
        set the channel id of a given edje object (only valid in setup
        commands for the inline edje object) to `CHANNEL ID` so it can
        be addressed later.
      * `message\nOPCODE ID\n[string|int|float|string_set|int_set|...`
        `float_set|string|int|string_float|string_int_set|...`
        `string_float_set]\n[message data]`
        send a message with opcode (integer) id `OPCODE ID` of one of
        the given types supported (`string`, `int`, `float`, etc.) and
        then with the given `message data` at the end.
        `message data` format depends on message type:
          * `string`:           `STRING1`
          * `int`:              `INT1`
          * `float`:            `FLOAT1`
          * `string_set`:       `STRING1\nSTRING2\nSTRING3...`
          * `int_set`:          `INT1\nINT2\nINT3...`
          * `float_set`:        `FLOAT1\nFLOAT2\nFLOAT3...`
          * `string_int`:       `STRING1\nINT1`
          * `string_float`:     `STRING1\nFLOAT1`
          * `string_int_set`:   `STRING1\nINT1\nINT2\nINT3...`
          * `string_float_set`: `STRING1\nFLOAT1\nFLOAT2\nFLOAT3...`

  * `ib`
    begin media replace sequence run

  * `ie`
    end media replace sequence run

  * `fr[PATH/FILE]`
    begin file send for a file named `PATH` / `FILE`

  * `fs[SIZE_BYTES]`
    set the size in bytes of a file send started with the above fr escape

  * `fd[CHECKSUM DATA]`
    block of data for the current file transfer with checksum as a
    string decimal which is the sum of every byte when taken as an
    unsigned char per byte. the checksum is a signed 32bit integer.
    the checksum is the sum of the data after escaping. 4 bits at a
    time per data byte, encoded with high bits in one byte then low
    bits, with the bits encoded as 4 bit value being `0x40` + 4 bit value
    per byte. `(@ == 0x0, A == 0x1, B == 0x2, ... N == 0xe, O == 0xf)`.
    so to rebuild a byte will be `(((bytes[0] - 0x40) & 0xf) << 4) | ((bytes[1] - 0x40) & 0xf)` per byte pair in the data block.

  * `fx`
    exit file send mode (normally at the end of the file or when it's
    complete)
