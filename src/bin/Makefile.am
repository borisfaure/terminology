MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = terminology

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@

terminology_SOURCES = \
col.c col.h \
config.c config.h \
keyin.c keyin.h \
main.c main.h \
options.c options.h \
options_font.c options_font.h \
termio.c termio.h \
termpty.c termpty.h \
utf8.c utf8.h \
win.c win.h
