A simple documentation to navige through the C files:

* `src/bin/about.c` handles the About widget
* `src/bin/col.c` is about the colors handled by the terminal
* `src/bin/config.c`: how the configuration is saved/loaded/updated
* `src/bin/controls.c`: the widget when a right-click is done on a terminal
* `src/bin/dbus.c`: all the D-Bus interactions
* `src/bin/extns.c` lists file extensions supported
* `src/bin/gravatar.c` hosts the code to show a Gravatar when hovering an email address
* `src/bin/ipc.c`: various IPC functions
* `src/bin/keyin.c`: handles key input
* `src/bin/main.c` host the main() function: setup/shutdown code
* `src/bin/media.c` handles media interactions like image popups, inlining movies
* `src/bin/miniview.c`: the miniview of the history
* `src/bin/options.c`: the settings widget
* `src/bin/options_behavior.c`: the settings panel that handles the Behaviors
* `src/bin/options_colors.c`: the settings panel about colors in the terminal
* `src/bin/options_elm.c`: the settings panel to configure Elementary
* `src/bin/options_font.c`: the settings panel to choose the Font
* `src/bin/options_helpers.c`: the settings panel on Helpers
* `src/bin/options_keys.c`: the settings panel to configure key bindings
* `src/bin/options_theme.c`: the settings panel to choose a theme
* `src/bin/options_themepv.c`: the widget that handles theme previews
* `src/bin/options_video.c`: the settings panel to configure video rendering
* `src/bin/options_wallpaper.c`: the settings panel to configure a wallpaper
* `src/bin/sel.c`: the tab selector
* `src/bin/termcmd.c` handles custom terminology commands
* `src/bin/termio.c`: the core term widget with the textgrid
* `src/bin/termiolink.c`: link detection in the terminal
* `src/bin/termpty.c`: the PTY interaction
* `src/bin/termptydbl.c`: code to hande double-width characters
* `src/bin/termptyesc.c`: escape codes parsing
* `src/bin/termptyext.c`: extented terminology escape handling
* `src/bin/termptygfx.c`: charset translations
* `src/bin/termptyops.c`: handling history
* `src/bin/termptysave.c`: compression of the backlog
* `src/bin/tyalpha.c`: the `tyalpha` tool
* `src/bin/tybg.c`: the `tybg` tool
* `src/bin/tycat.c`: the `tycat` tool
* `src/bin/tyls.c`: the `tyls` tool
* `src/bin/typop.c`: the `typop` tool
* `src/bin/tyq.c`: the `tyq` tool
* `src/bin/utf8.c`: handles conversion between Eina_Unicode and char *
* `src/bin/utils.c`: small utilitarian functions
* `src/bin/win.c`: handles the windows, splits, tabs
