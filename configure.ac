# Comments for autoconf: use dnl for comments, not as a macro
AC_CONFIG_MACRO_DIR([m4])
m4_define([GOLIATH_VERSION], regexp(m4_include([VERSION]), [version \([-.0-9A-Za-z]+\)], [\1]))

# Fallback for missing macros (no-op if not found)
m4_ifndef([AS_VAR_APPEND], [m4_define([AS_VAR_APPEND], [])])
m4_ifndef([AS_VAR_IF], [m4_define([AS_VAR_IF], [])])

AC_PREREQ([2.69])
AC_INIT([Goliath],[GOLIATH_VERSION],[goliath-devel@example.org],[goliath],[https://github.com/goliath-project/goliath])
AC_CONFIG_SRCDIR([upstream/wine/server/atom.c])
AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([tools])

# Check for required build tools
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
LT_INIT

dnl Check for macOS build
case $host in
    *-*-darwin*)
        build_macos=yes
        ;;
    *)
        build_macos=no
        ;;
esac
AM_CONDITIONAL([HAVE_MACOS], [test "x$build_macos" = "xyes"])

# Ensure pkg-config is available early
PKG_PROG_PKG_CONFIG

# Check for required development packages
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.32.0])
PKG_CHECK_MODULES([XML], [libxml-2.0 >= 2.9.0])

# Required system headers
AC_CHECK_HEADERS([sys/types.h sys/stat.h fcntl.h errno.h stdio.h stdlib.h string.h unistd.h], [], [
    AC_MSG_ERROR([Missing required system headers])
])

libwine_version="1.0"
libgoliath_version="1.0"
libdarling_version="1.0"
libatl_version="1.0"
libwsl_version="1.0"

AM_INIT_AUTOMAKE([foreign subdir-objects])

dnl *** Wine Compatibility Layer ***
AC_ARG_ENABLE([wine],
    AS_HELP_STRING([--enable-wine], [Enable Wine Windows compatibility layer (default: yes)]),
    [enable_wine=$enableval],
    [enable_wine=yes])

if test "x$enable_wine" = "xyes"; then
    AC_DEFINE([HAVE_WINE], [1], [Define if Wine support is enabled])
    PKG_CHECK_MODULES([WINE_DEPS], [
        x11
        xext
        xrandr
        xrender
        xinerama
        xcursor
        freetype2
    ])
fi

AM_CONDITIONAL([ENABLE_WINE], [test "x$enable_wine" = "xyes"])

dnl *** Darling macOS Compatibility Layer ***
AC_ARG_ENABLE([darling],
    AS_HELP_STRING([--enable-darling], [Enable Darling macOS compatibility layer (default: yes)]),
    [enable_darling=$enableval],
    [enable_darling=yes])

if test "x$enable_darling" = "xyes"; then
    AC_DEFINE([HAVE_DARLING], [1], [Define if Darling support is enabled])
    PKG_CHECK_MODULES([DARLING_DEPS], [
        libffi
        libxml-2.0
        openssl
        libbsd
    ])
fi

AM_CONDITIONAL([ENABLE_DARLING], [test "x$enable_darling" = "xyes"])

dnl *** Android Translation Layer ***
AC_ARG_ENABLE([atl],
    AS_HELP_STRING([--enable-atl], [Enable Android Translation Layer (default: yes)]),
    [enable_atl=$enableval],
    [enable_atl=yes])

if test "x$enable_atl" = "xyes"; then
    AC_DEFINE([HAVE_ATL], [1], [Define if ATL support is enabled])
    
    dnl Check for required Android/ATL dependencies
    PKG_CHECK_MODULES([ATL_DEPS], [
        libdrm
        gbm
        egl
        glesv2
    ], [], [
        AC_MSG_WARN([Some ATL dependencies not found. ATL support may be limited.])
        
        dnl Individual checks for partial functionality
        PKG_CHECK_MODULES([DRM], [libdrm], [
            AC_DEFINE([HAVE_DRM], [1], [Define if DRM is available])
            ATL_DEPS_CFLAGS="$ATL_DEPS_CFLAGS $DRM_CFLAGS"
            ATL_DEPS_LIBS="$ATL_DEPS_LIBS $DRM_LIBS"
        ], [AC_MSG_WARN([libdrm not found - GPU acceleration may be unavailable])])
        
        PKG_CHECK_MODULES([GBM], [gbm], [
            AC_DEFINE([HAVE_GBM], [1], [Define if GBM is available])
            ATL_DEPS_CFLAGS="$ATL_DEPS_CFLAGS $GBM_CFLAGS"
            ATL_DEPS_LIBS="$ATL_DEPS_LIBS $GBM_LIBS"
        ], [AC_MSG_WARN([gbm not found - GPU acceleration may be unavailable])])
        
        PKG_CHECK_MODULES([EGL], [egl], [
            AC_DEFINE([HAVE_EGL], [1], [Define if EGL is available])
            ATL_DEPS_CFLAGS="$ATL_DEPS_CFLAGS $EGL_CFLAGS"
            ATL_DEPS_LIBS="$ATL_DEPS_LIBS $EGL_LIBS"
        ], [AC_MSG_WARN([EGL not found - OpenGL ES support may be unavailable])])
        
        PKG_CHECK_MODULES([GLES2], [glesv2], [
            AC_DEFINE([HAVE_GLES2], [1], [Define if OpenGL ES 2.0 is available])
            ATL_DEPS_CFLAGS="$ATL_DEPS_CFLAGS $GLES2_CFLAGS"
            ATL_DEPS_LIBS="$ATL_DEPS_LIBS $GLES2_LIBS"
        ], [AC_MSG_WARN([OpenGL ES 2.0 not found - 3D acceleration will be unavailable])])
    ])
    
    dnl Look for Android SDK tools
    AC_PATH_PROG([ADB], [adb], [no])
    if test "x$ADB" != "xno"; then
        AC_DEFINE([HAVE_ADB], [1], [Define if Android Debug Bridge is available])
    fi
    
    AC_PATH_PROG([AAPT], [aapt], [no])
    if test "x$AAPT" != "xno"; then
        AC_DEFINE([HAVE_AAPT], [1], [Define if Android Asset Packaging Tool is available])
    fi
fi

AM_CONDITIONAL([ENABLE_ATL], [test "x$enable_atl" = "xyes"])

dnl *** WSL Integration ***
AC_ARG_ENABLE([wsl],
    AS_HELP_STRING([--enable-wsl], [Enable WSL integration (default: yes)]),
    [enable_wsl=$enableval],
    [enable_wsl=yes])

if test "x$enable_wsl" = "xyes"; then
    AC_DEFINE([HAVE_WSL], [1], [Define if WSL support is enabled])
    dnl Check for required kernel features
    AC_CHECK_HEADERS([linux/binfmts.h], [], [
        AC_MSG_ERROR([Missing required kernel headers for WSL support])
    ])
fi

AM_CONDITIONAL([ENABLE_WSL], [test "x$enable_wsl" = "xyes"])

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


dnl Check for flex (optional; warn if missing)
AC_CHECK_PROGS(FLEX,flex,none)
if test "$FLEX" = "none"; then
    AC_MSG_WARN([no suitable flex found. Some build steps may be skipped; install the 'flex' package for full functionality.])
    goliath_cv_recent_flex=no
else
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
fi
if test "$goliath_cv_recent_flex" = "no"; then
    AC_MSG_WARN([flex not found or out of date; some generated sources may be unavailable.])
fi


dnl Check for bison (optional; warn if missing)
AC_CHECK_PROGS(BISON,bison,none)
if test "$BISON" = "none"; then
    AC_MSG_WARN([no suitable bison found. Some build steps may be skipped; install the 'bison' package for full functionality.])
    wine_cv_recent_bison=no
else
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
fi
if test "$wine_cv_recent_bison" = "no"; then
    AC_MSG_WARN([bison not found or out of date; some generated parsers may be unavailable.])
fi

AC_CHECK_TOOL(STRIP,strip,strip)
AC_PROG_LN_S
PKG_PROG_PKG_CONFIG

dnl Output all makefiles
AC_CONFIG_FILES([
    Makefile
    include/Makefile
    libs/common/Makefile
    libs/compatibility/Makefile
    loader/Makefile
    server/Makefile
    tools/Makefile
    documentation/Makefile
])

if test "x$enable_wine" = "xyes"; then
    AC_CONFIG_SUBDIRS([upstream/wine])
fi

if test "x$enable_darling" = "xyes"; then
    AC_CONFIG_FILES([
        dlls/darling_coreaudio/Makefile
        dlls/darling_sandbox/Makefile
        libs/darling/Makefile
        libs/darling_framework/Makefile
    ])
fi

if test "x$enable_atl" = "xyes"; then
    AC_CONFIG_FILES([
        libs/atl_android/Makefile
        libs/atl_runtime/Makefile
        dlls/atl_bridge/Makefile
    ])
fi

if test "x$enable_wsl" = "xyes"; then
    AC_CONFIG_FILES([
        dlls/wsl/Makefile
        libs/wsl_core/Makefile
        libs/wsl_bridge/Makefile
    ])
fi

if test "x$enable_libretro" = "xyes"; then
    AC_CONFIG_FILES([
        libretro/Makefile
        libs/libretro_core/Makefile
    ])
fi

AC_OUTPUT

AC_MSG_NOTICE([Configuration summary for $PACKAGE_NAME $PACKAGE_VERSION:])
AC_MSG_NOTICE([  prefix:      ${prefix}])
AC_MSG_NOTICE([  sysconfdir:  ${sysconfdir}])
AC_MSG_NOTICE([  libdir:      ${libdir}])
AC_MSG_NOTICE([  includedir:  ${includedir}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([Components to build:])
AC_MSG_NOTICE([  Wine:     ${enable_wine}])
AC_MSG_NOTICE([  Darling:  ${enable_darling}])
AC_MSG_NOTICE([  ATL:      ${enable_atl}])
AC_MSG_NOTICE([  WSL:      ${enable_wsl}])
AC_MSG_NOTICE([  LibRetro: ${enable_libretro}])
AC_MSG_NOTICE([])
AC_MSG_NOTICE([Run 'make' to compile ${PACKAGE_NAME}])

dnl Local Variables:
dnl comment-start: "dnl "
dnl comment-end: ""
dnl comment-start-skip: "\\bdnl\\b\\s *"
dnl compile-command: "autoreconf --warnings=all"
dnl End:
