bin_PROGRAMS = goliath

goliath_SOURCES = \
    main.c \
    main.h \
    atl_main.c \
    preloader.c

if HAVE_MACOS
goliath_SOURCES += preloader_mac.c
endif

goliath_CFLAGS = \
    -I$(top_srcdir)/include \
    -DBIN_TO_DLLDIR=\"$(bindir)\" \
    -DLIB_TO_DLLDIR=\"$(libdir)\"

goliath_LDFLAGS = \
    -ldl \
    -lpthread

if HAVE_MACOS
goliath_LDFLAGS += -Wl,-image_base,0x7c000000
endif

desktopdir = $(datadir)/applications
desktop_DATA = wine.desktop

EXTRA_DIST = \
    wine.inf.in \
    wine.desktop \
    wine_info.plist.in \
    wine.man.in \
    wine.de.UTF-8.man.in \
    wine.fr.UTF-8.man.in \
    wine.pl.UTF-8.man.in

CLEANFILES = wine.inf
