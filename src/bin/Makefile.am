MAINTAINERCLEANFILES = Makefile.in

EDJE_CODEGEN = @edje_codegen@

background_generated.c background_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="background" $(top_builddir)/data/themes/default.edj \
terminology/background $(builddir)/background_generated.c $(builddir)/background_generated.h

cursor_generated.c cursor_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="cursor" $(top_builddir)/data/themes/default.edj \
terminology/cursor $(builddir)/cursor_generated.c $(builddir)/cursor_generated.h

selection_generated.c selection_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="selection" $(top_builddir)/data/themes/default.edj \
terminology/selection $(builddir)/selection_generated.c $(builddir)/selection_generated.h

mediactrl_generated.c mediactrl_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="mediactrl" $(top_builddir)/data/themes/default.edj \
terminology/mediactrl $(builddir)/mediactrl_generated.c $(builddir)/mediactrl_generated.h

mediabusy_generated.c mediabusy_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="mediabusy" $(top_builddir)/data/themes/default.edj \
terminology/mediabusy $(builddir)/mediabusy_generated.c $(builddir)/mediabusy_generated.h

fontpreview_generated.c fontpreview_generated.h: $(top_builddir)/data/themes/default.edj
	$(EDJE_CODEGEN) --prefix="fontpreview" $(top_builddir)/data/themes/default.edj \
terminology/fontpreview $(builddir)/fontpreview_generated.c $(builddir)/fontpreview_generated.h

bin_PROGRAMS = terminology

terminology_CPPFLAGS = -I. \
-DPACKAGE_BIN_DIR=\"$(bindir)\" -DPACKAGE_LIB_DIR=\"$(libdir)\" \
-DPACKAGE_DATA_DIR=\"$(pkgdatadir)\" @TERMINOLOGY_CFLAGS@

terminology_LDADD = @TERMINOLOGY_LIBS@

BUILT_SOURCES = \
background_generated.c background_generated.h \
cursor_generated.c cursor_generated.h \
fontpreview_generated.c fontpreview_generated.h \
selection_generated.c selection_generated.h \
mediactrl_generated.c mediactrl_generated.h \
mediabusy_generated.c mediabusy_generated.h

terminology_SOURCES = \
background_generated.c background_generated.h \
cursor_generated.c cursor_generated.h \
fontpreview_generated.c fontpreview_generated.h \
selection_generated.c selection_generated.h \
mediactrl_generated.c mediactrl_generated.h \
mediabusy_generated.c mediabusy_generated.h \
private.h \
about.c about.h \
col.c col.h \
config.c config.h \
controls.c controls.h \
keyin.c keyin.h \
main.c main.h \
media.c media.h \
options.c options.h \
options_behavior.c options_behavior.h \
options_colors.c options_colors.h \
options_font.c options_font.h \
options_helpers.c options_helpers.h \
options_video.c options_video.h \
options_theme.c options_theme.h \
options_wallpaper.c options_wallpaper.h \
termio.c termio.h \
termcmd.c termcmd.h \
termiolink.c termiolink.h \
termpty.c termpty.h \
termptydbl.c termptydbl.h \
termptyesc.c termptyesc.h \
termptyops.c termptyops.h \
termptygfx.c termptygfx.h \
termptyext.c termptyext.h \
utf8.c utf8.h \
win.c win.h \
utils.c utils.h
