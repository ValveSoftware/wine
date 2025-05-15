AC_DEFUN([WINE_CHECK_DEFINE], [
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM([[#ifndef $1
    #error "$1 not defined"
    #endif]], [[]])
  ],
  [AC_DEFINE_UNQUOTED([$1], 1, [Define if $1 is available])],
  [])
])
