# Comments for autoconf: use dnl for comments, not as a macro
AC_CONFIG_MACRO_DIR([m4])
m4_define([GOLIATH_VERSION], regexp(m4_include([VERSION]), [version \([-.0-9A-Za-z]+\)], [\1]))

# Fallback for missing macros (no-op if not found)
m4_ifndef([AS_VAR_APPEND], [m4_define([AS_VAR_APPEND], [])])
m4_ifndef([AS_VAR_IF], [m4_define([AS_VAR_IF], [])])

AC_PREREQ([2.63b])
AC_INIT([Goliath],[GOLIATH_VERSION],[goliath-devel@example.org],[goliath],[https://github.com/goliath-project/goliath])
AC_CONFIG_SRCDIR([server/atom.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([tools])

libwine_version="1.0"
libgoliath_version="1.0"

AM_INIT_AUTOMAKE([foreign subdir-objects])
LT_INIT

dnl *** Goliath LibRetro Support ***
AC_ARG_ENABLE([libretro],
    AS_HELP_STRING([--enable-libretro], [Enable LibRetro support for legacy ROM emulation (default: yes)]),
    [enable_libretro=$enableval],
    [enable_libretro=yes])

if test "x$enable_libretro" = "xyes"; then
    AC_DEFINE([HAVE_LIBRETRO], [1], [Define if LibRetro support is enabled])
    
    dnl Check for libdl (required for dynamic loading of LibRetro cores)
    AC_CHECK_LIB([dl], [dlopen], [], [
        AC_MSG_ERROR([libdl is required for LibRetro support. Install libdl-dev or disable LibRetro with --disable-libretro])
    ])
    
    dnl Check for common LibRetro core directories
    AC_MSG_CHECKING([for LibRetro cores directory])
    LIBRETRO_CORES_DIR=""
    for dir in /usr/lib/libretro /usr/local/lib/libretro /opt/libretro/cores; do
        if test -d "$dir"; then
            LIBRETRO_CORES_DIR="$dir"
            break
        fi
    done
    
    if test -n "$LIBRETRO_CORES_DIR"; then
        AC_MSG_RESULT([$LIBRETRO_CORES_DIR])
        AC_DEFINE_UNQUOTED([DEFAULT_LIBRETRO_CORES_DIR], ["$LIBRETRO_CORES_DIR"], [Default LibRetro cores directory])
    else
        AC_MSG_RESULT([not found - will use runtime detection])
        AC_DEFINE_UNQUOTED([DEFAULT_LIBRETRO_CORES_DIR], ["/usr/lib/libretro"], [Default LibRetro cores directory])
    fi
fi

AM_CONDITIONAL([ENABLE_LIBRETRO], [test "x$enable_libretro" = "xyes"])

dnl Check for flex
AC_CHECK_PROGS(FLEX,flex,none)
if test "$FLEX" = "none"
then
  AC_MSG_ERROR([no suitable flex found. Please install the 'flex' package.])
fi
AC_CACHE_CHECK([whether flex is recent enough],goliath_cv_recent_flex,
    [cat >conftest.l <<EOF
%top{
#include "prediluvian.h"
}
%%
EOF
    if $FLEX -t conftest.l >/dev/null 2>&AS_MESSAGE_LOG_FD
    then
      goliath_cv_recent_flex=yes
    else
      goliath_cv_recent_flex=no
    fi])
test $goliath_cv_recent_flex != no || AC_MSG_ERROR([Your flex version is too old. Please install flex version 2.5.33 or newer.])

dnl Check for bison
AC_CHECK_PROGS(BISON,bison,none)
if test "$BISON" = "none"
then
  AC_MSG_ERROR([no suitable bison found. Please install the 'bison' package.])
fi
AC_CACHE_CHECK([whether bison is recent enough],wine_cv_recent_bison,
    [cat >conftest.y <<EOF
%define parse.error verbose
%%
empty: ;
EOF
    if $BISON conftest.y >/dev/null 2>&AS_MESSAGE_LOG_FD
    then
      wine_cv_recent_bison=yes
    else
      wine_cv_recent_bison=no
    fi])
test $wine_cv_recent_bison != no || AC_MSG_ERROR([Your bison version is too old. Please install bison version 3.0 or newer.])

AC_CHECK_TOOL(STRIP,strip,strip)
AC_PROG_LN_S
WINE_PATH_PKG_CONFIG

AC_OUTPUT

AS_ECHO("
$as_me: Finished.  Do '${MAKE-make}' to compile Goliath.
") >&AS_MESSAGE_FD

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "autoreconf --warnings=all"
dnl End: