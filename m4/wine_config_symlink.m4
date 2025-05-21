AC_DEFUN([WINE_CONFIG_SYMLINK], [
  m4_ifval([$3], [
    if $3; then
      :
    fi
  ])
])
