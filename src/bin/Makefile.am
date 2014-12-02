AUTOMAKE_OPTIONS = subdir-objects
MAINTAINERCLEANFILES = Makefile.in

bin_PROGRAMS = terminology tybg tyalpha typop tyq tycat tyls

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@ @ELDBUS_LIBS@
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
term_container.h \
utils.c utils.h \
dbus.c dbus.h \
extns.c extns.h \
app_server.c app_server.h \
app_server_eet.c app_server_eet.h \
gravatar.c gravatar.h \
tty_keys.h

tybg_SOURCES = \
tybg.c

tybg_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"

tybg_LDADD =

tyalpha_SOURCES = \
tyalpha.c

tyalpha_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"

tyalpha_LDADD =

typop_SOURCES = \
typop.c

typop_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"

typop_LDADD =

tyq_SOURCES = \
tyq.c

tyq_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\"

tyq_LDADD =

tycat_SOURCES = \
tycat.c \
extns.c extns.h

tycat_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tycat_LDADD = @TERMINOLOGY_LIBS@

tyls_SOURCES = \
tyls.c \
extns.h

tyls_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

tyls_LDADD = @TERMINOLOGY_LIBS@
