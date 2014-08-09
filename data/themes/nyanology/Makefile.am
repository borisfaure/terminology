MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = images images-sources

EDJE_CC = @edje_cc@
EDJE_FLAGS_VERBOSE_ =
EDJE_FLAGS_VERBOSE_0 =
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_VERBOSE_$(V)) -id $(top_srcdir)/data/themes/nyanology/images -id $(top_srcdir)/data/themes/images -fd $(top_srcdir)/data/fonts -sd $(top_srcdir)/data/themes/sounds

filesdir = $(pkgdatadir)/themes
files_DATA = \
nyanology.edj

AM_V_EDJ = $(am__v_EDJ_$(V))
am__v_EDJ_ = $(am__v_EDJ_$(AM_DEFAULT_VERBOSITY))
am__v_EDJ_0 = @echo "  EDJ     " $@;

EXTRA_DIST = \
README.md \
about.edc \
nyancursor.edc \
nyanology.edc

nyanology.edj: Makefile nyanology.edc about.edc nyancursor.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/nyanology/nyanology.edc \
	$(top_builddir)/data/themes/nyanology/nyanology.edj

clean-local:
	rm -f *.edj
