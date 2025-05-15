AC_DEFUN([WINE_CONFIG_SYMLINK], [
  AC_CONFIG_COMMANDS([$1], [
    rm -f $1 && ln -s $2 $1
  ])
])
