MAINTAINERCLEANFILES = Makefile.in

SUBDIRS = images sounds

EDJE_CC = @edje_cc@
EDJE_FLAGS_VERBOSE_ =
EDJE_FLAGS_VERBOSE_0 =
EDJE_FLAGS_VERBOSE_1 = -v
EDJE_FLAGS = $(EDJE_FLAGS_VERBOSE_$(V)) -id $(top_srcdir)/data/themes/images -fd $(top_srcdir)/data/fonts -sd $(top_srcdir)/data/themes/sounds

filesdir = $(pkgdatadir)/themes
files_DATA = \
default.edj \
mild.edj \
black.edj \
solarized.edj \
solarized_light.edj \
mustang.edj \
base16_ocean_dark.edj

AM_V_EDJ = $(am__v_EDJ_$(V))
am__v_EDJ_ = $(am__v_EDJ_$(AM_DEFAULT_VERBOSITY))
am__v_EDJ_0 = @echo "  EDJ   " $@;

EXTRA_DIST = \
default.edc \
mild.edc \
black.edc \
mustang.edc \
base16_ocean_dark.edc \
solarized.edc \
solarized_light.edc \
default_colors.in.edc

default.edj: Makefile default.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/default.edc \
	$(top_builddir)/data/themes/default.edj

mild.edj: Makefile mild.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/mild.edc \
	$(top_builddir)/data/themes/mild.edj

black.edj: Makefile black.edc mild.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/black.edc \
	$(top_builddir)/data/themes/black.edj

solarized.edj: Makefile solarized.edc default.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/solarized.edc \
	$(top_builddir)/data/themes/solarized.edj

solarized_light.edj: Makefile solarized_light.edc default.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/solarized_light.edc \
	$(top_builddir)/data/themes/solarized_light.edj

mustang.edj: Makefile mustang.edc mild.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/mustang.edc \
	$(top_builddir)/data/themes/mustang.edj

base16_ocean_dark.edj: Makefile base16_ocean_dark.edc mild.edc default_colors.in.edc
	$(AM_V_EDJ)$(EDJE_CC) $(EDJE_FLAGS) \
	$(top_srcdir)/data/themes/base16_ocean_dark.edc \
	$(top_builddir)/data/themes/base16_ocean_dark.edj

clean-local:
	rm -f *.edj
