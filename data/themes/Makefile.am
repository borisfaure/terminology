MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = images

EDJE_CC = @edje_cc@
EDJE_FLAGS_VERBOSE_ =
EDJE_FLAGS_VERBOSE_0 =
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_$(V)) -id $(top_srcdir)/data/themes/images -fd $(top_srcdir)/data/fonts

filesdir = $(pkgdatadir)/themes
files_DATA = \
default.edj \
mild.edj \
black.edj \
solarized.edj \
mustang.edj

EXTRA_DIST = default.edc mild.edc black.edc solarized.edc mustang.edc default_colors.in.edc

default.edj: Makefile default.edc
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/default.edc \
	$(top_builddir)/data/themes/default.edj

mild.edj: Makefile mild.edc
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/mild.edc \
	$(top_builddir)/data/themes/mild.edj

black.edj: Makefile black.edc mild.edc
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/black.edc \
	$(top_builddir)/data/themes/black.edj

solarized.edj: Makefile solarized.edc
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/solarized.edc \
	$(top_builddir)/data/themes/solarized.edj

mustang.edj: Makefile mustang.edc
	$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/mustang.edc \
	$(top_builddir)/data/themes/mustang.edj

clean-local:
	rm -f *.edj
