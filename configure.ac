dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT([terminology], [1.1.1], [enlightenment-devel@lists.sourceforge.net])
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

efl_version="1.8.0"
AC_SUBST(efl_version)


m4_ifdef([AM_GNU_GETTEXT_VERSION], [
AM_GNU_GETTEXT_VERSION([0.18])
])

m4_ifdef([AM_GNU_GETTEXT], [
AM_GNU_GETTEXT([external], [need-ngettext])
po_makefile_in=po/Makefile.in
have_po="yes"
],[
have_po="no"
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

# check for fuzzing
want_fuzzing="fuzzing"
AC_ARG_ENABLE([fuzzing],
   [AS_HELP_STRING([--enable-fuzzing],[Enable fuzzing on terminology. Binaries
    are not suited for production. @<:@default=disabled@:>@])],
   [
    if test "x${enableval}" = "xyes" ; then
       want_fuzzing="yes"
       AC_DEFINE([ENABLE_FUZZING], [1], [Fuzzing enabled])
       AC_SUBST([want_fuzzing])
    else
       want_fuzzing="no"
    fi
   ],
   [want_fuzzing="no"])

AM_CONDITIONAL([ENABLE_FUZZING], [test "${want_fuzzing}" = "yes"])

PKG_CHECK_MODULES([TERMINOLOGY], [${requirements}])

AC_CHECK_FUNCS(mkstemps)

orig_cflags=$CFLAGS
CFLAGS+=" $TERMINOLOGY_CFLAGS "
LDFLAGS+=" $TERMINOLOGY_LDFLAGS $TERMINOLOGY_LIBS "
AC_MSG_CHECKING([for ecore_con_url_head])
AC_LINK_IFELSE(
   [
    AC_LANG_PROGRAM(
      [[
       #include <Ecore_Con.h>
      ]],
     [[
        ecore_con_url_head(NULL);
     ]])
   ],
   [
     AC_MSG_RESULT([yes])
     AC_DEFINE([HAVE_ECORE_CON_URL_HEAD], [1], [define to 1 if ecore_con_url_head is available])
   ],
   [
     AC_MSG_RESULT([no])
   ]
)
CFLAGS=$orig_cflags

EFL_WITH_BIN([edje], [edje-cc], [edje_cc])

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
echo "  gettext.............: $have_po"
echo "  fuzzing.............: $want_fuzzing"
echo
