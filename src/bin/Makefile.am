MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = terminology

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@ @ECORE_IMF_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@ @ECORE_IMF_LIBS@

terminology_SOURCES = \
private.h \
col.c col.h \
config.c config.h \
keyin.c keyin.h \
main.c main.h \
media.c media.h \
options.c options.h \
options_behavior.c options_behavior.h \
options_font.c options_font.h \
options_video.c options_video.h \
termio.c termio.h \
termpty.c termpty.h \
utf8.c utf8.h \
win.c win.h
