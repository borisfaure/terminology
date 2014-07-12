dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT([terminology], [0.6.99], [enlightenment-devel@lists.sourceforge.net])
AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([terminology_config.h])

AM_INIT_AUTOMAKE([1.6 dist-xz])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS
AC_PROG_CC
AM_PROG_CC_C_O
AC_C___ATTRIBUTE__

efl_version="1.7.0"
AC_SUBST(efl_version)


m4_ifdef([AM_GNU_GETTEXT_VERSION], [
AM_GNU_GETTEXT_VERSION([0.18.3])
])

m4_ifdef([AC_GNU_GETTEXT], [
AC_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
m4_ifdef([AM_GNU_GETTEXT], [
AM_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
have_po="no"
])
])
AC_SUBST(LTLIBINTL)
if test "x$LIBINTL" = "x"; then
  LIBINTL="$INTLLIBS"
fi

if test "x${POSUB}" = "x" ; then
   have_po="no"
fi

AM_CONDITIONAL([HAVE_PO], [test "x${have_po}" = "xyes"])


requirements="\
   elementary >= ${efl_version} \
   eina >= ${efl_version} \
   eet >= ${efl_version} \
   evas >= ${efl_version} \
   ecore >= ${efl_version} \
   ecore-evas >= ${efl_version} \
   ecore-file >= ${efl_version} \
   edje >= ${efl_version} \
   emotion >= ${efl_version} \
   ecore-input >= ${efl_version} \
   ecore-imf >= ${efl_version} \
   ecore-imf-evas >= ${efl_version} \
   ecore-ipc >= ${efl_version} \
   efreet >= ${efl_version} \
   ethumb_client >= ${efl_version} \
   "

EFL_ENABLE_EO_API_SUPPORT
EFL_ENABLE_BETA_API_SUPPORT

PKG_CHECK_MODULES([TERMINOLOGY], [${requirements}])

PKG_CHECK_MODULES([ELDBUS],
                  [eldbus],
                  [
                   AC_DEFINE(HAVE_ELDBUS, 1, [DBUS support])
                   have_eldbus="yes"
                   ],
                   [have_eldbus="no"]
                   )

AC_CHECK_FUNCS(mkstemps)

EFL_WITH_BIN([edje], [edje-cc], [edje_cc])

with_max_log_level="EINA_LOG_LEVEL_ERR"
AC_ARG_WITH(maximum-log-level,
   [AC_HELP_STRING([--with-maximum-log-level=NUMBER],
                   [limit terminology log level to the given number, any call to EINA_LOG() with values greater than this will be compiled out, ignoring runtime settings, but saving function calls.])],
   [
    if test "x${withval}" != "xno"; then
       if echo "${withval}" | grep -E '^[[0-9]]+$' >/dev/null 2>/dev/null; then
          AC_MSG_NOTICE([ignoring any EINA_LOG() with level greater than ${withval}])
          with_max_log_level="${withval}"
       else
          AC_MSG_ERROR([--with-maximum-log-level takes a decimal number, got "${withval}" instead.])
       fi
    fi
    ], [:])

AC_DEFINE_UNQUOTED(EINA_LOG_LEVEL_MAXIMUM, ${with_max_log_level}, [if set, logging is limited to this amount.])

AC_CONFIG_FILES([
terminology.spec
pkgbuild/PKGBUILD
])

AC_OUTPUT([
Makefile
src/Makefile
src/bin/Makefile
data/Makefile
data/desktop/Makefile
data/icons/Makefile
data/backgrounds/Makefile
data/images/Makefile
data/fonts/Makefile
data/themes/Makefile
data/themes/images/Makefile
data/themes/sounds/Makefile
data/themes/nyanology/Makefile
data/themes/nyanology/images/Makefile
data/themes/nyanology/images-sources/Makefile
man/Makefile
$po_makefile_in
])

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
echo "Features:"
echo "  dbus................: $have_eldbus"
echo "  gettext.............: $have_po"
echo
