dnl Copyright (C) 2010 Vincent Torri <vtorri at univ-evry dot fr>
dnl That code is public domain and can be freely used or copied.

dnl Macro that check if a binary is built or not

dnl Usage: EFL_ENABLE_BIN(binary)
dnl Call AC_SUBST(BINARY_PRG) (BINARY is the uppercase of binary, - being transformed into _)
dnl Define have_binary (- is transformed into _)
dnl Define conditional BUILD_BINARY (BINARY is the uppercase of binary, - being transformed into _)

AC_DEFUN([EFL_ENABLE_BIN],
[

m4_pushdef([UP], m4_translit([[$1]], [-a-z], [_A-Z]))dnl
m4_pushdef([DOWN], m4_translit([[$1]], [-A-Z], [_a-z]))dnl

have_[]m4_defn([DOWN])="yes"

dnl configure option

AC_ARG_ENABLE([$1],
   [AC_HELP_STRING([--disable-$1], [disable building of ]DOWN)],
   [
    if test "x${enableval}" = "xyes" ; then
       have_[]m4_defn([DOWN])="yes"
    else
       have_[]m4_defn([DOWN])="no"
    fi
   ])

AC_MSG_CHECKING([whether to build ]DOWN[ binary])
AC_MSG_RESULT([$have_[]m4_defn([DOWN])])

if test "x$have_[]m4_defn([DOWN])" = "xyes"; then
   UP[]_PRG=DOWN[${EXEEXT}]
fi

AC_SUBST(UP[]_PRG)

AM_CONDITIONAL(BUILD_[]UP, test "x$have_[]m4_defn([DOWN])" = "xyes")

AS_IF([test "x$have_[]m4_defn([DOWN])" = "xyes"], [$2], [$3])

])


dnl Macro that check if a binary is built or not

dnl Usage: EFL_WITH_BIN(package, binary, default_value)
dnl Call AC_SUBST(_binary) (_binary is the lowercase of binary, - being transformed into _ by default, or the value set by the user)

AC_DEFUN([EFL_WITH_BIN],
[

m4_pushdef([DOWN], m4_translit([[$2]], [-A-Z], [_a-z]))dnl

dnl configure option

AC_ARG_WITH([$2],
   [AC_HELP_STRING([--with-$2=PATH], [specify a specific path to ]DOWN[ @<:@default=$3@:>@])],
   [_efl_with_binary=${withval}],
   [_efl_with_binary=$(pkg-config --variable=prefix $1)/bin/$3])

DOWN=${_efl_with_binary}
AC_MSG_NOTICE(DOWN[ set to ${_efl_with_binary}])

with_binary_[]m4_defn([DOWN])=${_efl_with_binary}

AC_SUBST(DOWN)

])
