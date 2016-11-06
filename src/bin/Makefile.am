AUTOMAKE_OPTIONS = subdir-objects
MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = terminology tybg tyalpha typop tyq tycat tyls
if ENABLE_FUZZING
bin_PROGRAMS += tyfuzz
endif

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@
if HAVE_PO
terminology_CPPFLAGS += -DLOCALEDIR=\"$(localedir)\"
terminology_LDADD += @LIBINTL@
endif




terminology_SOURCES = \
private.h \
about.c about.h \
col.c col.h \
config.c config.h \
controls.c controls.h \
ipc.c ipc.h \
keyin.c keyin.h \
main.c main.h \
media.c media.h \
options.c options.h \
options_font.c options_font.h \
options_theme.c options_theme.h \
options_themepv.c options_themepv.h \
options_wallpaper.c options_wallpaper.h \
options_colors.c options_colors.h \
options_behavior.c options_behavior.h \
options_keys.c options_keys.h \
options_helpers.c options_helpers.h \
options_video.c options_video.h \
options_elm.c options_elm.h \
sel.c sel.h \
miniview.c miniview.h \
termio.c termio.h \
termcmd.c termcmd.h \
term_container.h \
termiolink.c termiolink.h \
termpty.c termpty.h \
termptydbl.c termptydbl.h \
termptyesc.c termptyesc.h \
termptyops.c termptyops.h \
termptygfx.c termptygfx.h \
termptyext.c termptyext.h \
termptysave.c termptysave.h \
lz4/lz4.c lz4/lz4.h \
md5/md5.c md5/md5.h \
utf8.c utf8.h \
win.c win.h \
utils.c utils.h \
extns.c extns.h \
gravatar.c gravatar.h \
tty_keys.h \
sb.c sb.h

tybg_SOURCES = \
tycommon.c tycommon.h \
tybg.c

tybg_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tybg_LDADD =

tyalpha_SOURCES = \
tycommon.c \
tyalpha.c

tyalpha_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tyalpha_LDADD =

typop_SOURCES = \
tycommon.c \
typop.c

typop_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

typop_LDADD =

tyq_SOURCES = \
tycommon.c \
tyq.c

tyq_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tyq_LDADD =

tycat_SOURCES = \
tycat.c \
tycommon.c \
extns.c extns.h

tycat_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tycat_LDADD = @TERMINOLOGY_LIBS@

tyls_SOURCES = \
tyls.c \
tycommon.c \
extns.h

tyls_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tyls_LDADD = @TERMINOLOGY_LIBS@

if ENABLE_FUZZING
tyfuzz_SOURCES = \
termptyesc.c \
termptysave.c \
termptyops.c \
termptydbl.c \
termptyext.c \
termptygfx.c \
termpty.c \
config.c \
col.c \
tyfuzz.c

tyfuzz_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tyfuzz_LDADD = @TERMINOLOGY_LIBS@
endif
