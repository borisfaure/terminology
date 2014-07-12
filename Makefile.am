ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = src data man

if HAVE_PO

SUBDIRS += po

endif

MAINTAINERCLEANFILES = \
ABOUT-NLS* \
Makefile.in \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.bz2 \
aclocal.m4 \
config.guess \
config.rpath \
terminology_config.h.in \
config.sub \
configure \
compile \
depcomp \
install-sh \
ltconfig \
ltmain.sh \
missing \
mkinstalldirs \
stamp-h.in \
stamp-h \
m4/libtool.m4 \
m4/lt~obsolete.m4 \
m4/ltoptions.m4 \
m4/ltsugar.m4 \
m4/ltversion.m4

if HAVE_PO

MAINTAINERCLEANFILES += \
po/boldquot.sed \
po/en@boldquot.header \
po/en@quot.header \
po/insert-header.sin \
po/Makefile.in.in \
po/Makevars.template \
po/quot.sed \
po/remove-potcdate.sin \
po/Rules-quot

endif

EXTRA_DIST = README AUTHORS COPYING autogen.sh config.rpath terminology.spec pkgbuild/PKGBUILD

.PHONY: doc

doc:
	@echo "Empty doc target until we have real documentation."
