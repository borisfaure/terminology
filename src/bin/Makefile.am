MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = terminology

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@

terminology_SOURCES = \
private.h \
col.c col.h \
config.c config.h \
controls.c controls.h \
keyin.c keyin.h keycomp.h \
main.c main.h \
media.c media.h \
options.c options.h \
options_behavior.c options_behavior.h \
options_font.c options_font.h \
options_video.c options_video.h \
options_theme.c options_theme.h \
options_wallpaper.c options_wallpaper.h \
termio.c termio.h \
termpty.c termpty.h \
utf8.c utf8.h \
win.c win.h \
utils.c utils.h
