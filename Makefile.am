ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}

SUBDIRS = src data man

if HAVE_PO

SUBDIRS += po

endif

MAINTAINERCLEANFILES = \
ABOUT-NLS* \
Makefile.in \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.gz \
$(PACKAGE_TARNAME)-$(PACKAGE_VERSION).tar.xz \
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
po/Rules-quot \
intl/ \
m4/codeset.m4 \
m4/fcntl-o.m4 \
m4/glibc2.m4 \
m4/glibc21.m4 \
m4/intdiv0.m4 \
m4/intl.m4 \
m4/intldir.m4 \
m4/intlmacosx.m4 \
m4/intmax.m4 \
m4/inttypes-pri.m4 \
m4/inttypes_h.m4 \
m4/lcmessage.m4 \
m4/lock.m4 \
m4/longlong.m4 \
m4/printf-posix.m4 \
m4/size_max.m4 \
m4/stdint_h.m4 \
m4/threadlib.m4 \
m4/uintmax_t.m4 \
m4/visibility.m4 \
m4/wchar_t.m4 \
m4/wint_t.m4 \
m4/xsize.m4 \
po/Makevars.template \
m4/gettext.m4 \
m4/iconv.m4 \
m4/lib-ld.m4 \
m4/lib-link.m4 \
m4/lib-prefix.m4 \
m4/nls.m4 \
m4/po.m4 \
m4/progtest.m4


endif

EXTRA_DIST = README AUTHORS COPYING autogen.sh config.rpath terminology.spec pkgbuild/PKGBUILD

.PHONY: doc

doc:
	@echo "Empty doc target until we have real documentation."
