dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT([terminology], [0.1.0], [enlightenment-devel@lists.sourceforge.net])
AC_PREREQ([2.60])
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([terminology_config.h])

AM_INIT_AUTOMAKE([1.6 dist-bzip2])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_USE_SYSTEM_EXTENSIONS
AM_PROG_CC_STDC
AM_PROG_CC_C_O
AC_C___ATTRIBUTE__

requirements="\
   elementary >= 1.0.99 \
   eina >= 1.2.99 \
   eet >= 1.6.99 \
   evas >= 1.2.99 \
   ecore >= 1.2.99 \
   edje >= 1.2.99 \
   emotion >= 1.0.99 \
   "

PKG_CHECK_MODULES([TERMINOLOGY], [${requirements}])

EFL_WITH_BIN([edje], [edje-cc], [edje_cc])

have_ecore_imf="no"
want_ecore_imf="auto"

AC_ARG_ENABLE([ecore-imf],
   [AC_HELP_STRING([--disable-ecore-imf], [disable ecore-imf (Input Method Framework) usage. @<:@default=enabled@:>@])],
   [want_ecore_imf=$enableval], [])

AM_CONDITIONAL(HAVE_ECORE_IMF, false)
if test "x$want_ecore_imf" != "xno"; then
   PKG_CHECK_MODULES([ECORE_IMF],
       [ecore-imf >= 1.2.0 ecore-imf-evas >= 1.2.0],
       [
          AC_DEFINE([HAVE_ECORE_IMF], [1], [Input Method Support])
          AM_CONDITIONAL(HAVE_ECORE_IMF, true)
          have_ecore_imf="yes"
          requirements="ecore-imf-evas >= 1.2.0 ecore-imf >= 1.2.0 ${requirements}"
       ],
       [have_ecore_imf="no"])

   if test "x$want_ecore_imf" = "xyes" -a "x$have_ecore_imf" = "xno"; then
      AC_MSG_ERROR([ecore-imf support requested but not found by pkg-config.])
   fi
fi

with_max_log_level="<unset>"
AC_ARG_WITH(maximum-log-level,
   [AC_HELP_STRING([--with-maximum-log-level=NUMBER],
                   [limit terminology log level to the given number, any call to EINA_LOG() with values greater than this will be compiled out, ignoring runtime settings, but saving function calls.])],
   [
    if test "x${withval}" != "xno"; then
       if echo "${withval}" | grep -E '^[[0-9]]+$' >/dev/null 2>/dev/null; then
          AC_MSG_NOTICE([ignoring any EINA_LOG() with level greater than ${withval}])
          AC_DEFINE_UNQUOTED(EINA_LOG_LEVEL_MAXIMUM, ${withval}, [if set, logging is limited to this amount.])
          with_max_log_level="${withval}"
       else
          AC_MSG_ERROR([--with-maximum-log-level takes a decimal number, got "${withval}" instead.])
       fi
    fi
    ], [:])

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
echo "Configuration Options Summary:"
echo "  Ecore IMF................: $have_ecore_imf"
echo
echo "Compilation................: make (or gmake)"
echo "  CPPFLAGS.................: $CPPFLAGS"
echo "  CFLAGS...................: $CFLAGS"
echo "  LDFLAGS..................: $LDFLAGS"
echo
echo "Installation...............: make install (as root if needed, with 'su' or 'sudo')"
echo "  prefix...................: $prefix"
echo
