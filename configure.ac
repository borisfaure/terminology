dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT([terminology], [0.1.0], [enlightenment-devel@lists.sourceforge.net])
AC_PREREQ([2.52])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([terminology_config.h])

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AM_PROG_CC_STDC
AM_PROG_CC_C_O

requirements="\
   elementary >= 1.0.99 \
   eina >= 1.2.99 \
   eet >= 1.6.99 \
   evas >= 1.2.99 \
   ecore >= 1.2.99 \
   edje >= 1.2.99"

PKG_CHECK_MODULES([TERMINOLOGY], [${requirements}])

EFL_WITH_BIN([edje], [edje-cc], [edje_cc])

AC_CONFIG_FILES([
Makefile
src/Makefile
src/bin/Makefile
data/Makefile
data/desktop/Makefile
data/icons/Makefile
data/images/Makefile
data/fonts/Makefile
data/themes/Makefile
data/themes/images/Makefile
])

AC_OUTPUT

#####################################################################
## Info

echo
echo
echo
echo "------------------------------------------------------------------------"
echo "$PACKAGE $VERSION"
echo "------------------------------------------------------------------------"
echo
echo "Compilation................: make (or gmake)"
echo "  CPPFLAGS.................: $CPPFLAGS"
echo "  CFLAGS...................: $CFLAGS"
echo "  LDFLAGS..................: $LDFLAGS"
echo
echo "Installation...............: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...................: $prefix"
echo
