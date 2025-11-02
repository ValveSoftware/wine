AUTOMAKE_OPTIONS = foreign

noinst_LTLIBRARIES = libgoliath_core.la

libgoliath_core_la_SOURCES = \
    core.c

noinst_HEADERS = \
    $(top_srcdir)/include/goliath/core.h \
    $(top_srcdir)/include/goliath/runtime.h

AM_CFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS)
AM_CPPFLAGS = -I$(top_srcdir)/include $(GLIB_CFLAGS)

if ENABLE_WINE
AM_CFLAGS += -DHAVE_WINE $(WINE_DEPS_CFLAGS)
AM_CPPFLAGS += -DHAVE_WINE $(WINE_DEPS_CFLAGS)
endif

if ENABLE_DARLING
AM_CFLAGS += -DHAVE_DARLING $(DARLING_DEPS_CFLAGS)
AM_CPPFLAGS += -DHAVE_DARLING $(DARLING_DEPS_CFLAGS)
endif

if ENABLE_ATL
AM_CFLAGS += -DHAVE_ATL $(ATL_DEPS_CFLAGS)
AM_CPPFLAGS += -DHAVE_ATL $(ATL_DEPS_CFLAGS)
endif

if ENABLE_WSL
AM_CFLAGS += -DHAVE_WSL
AM_CPPFLAGS += -DHAVE_WSL
endif

if ENABLE_LIBRETRO
AM_CFLAGS += -DHAVE_LIBRETRO
AM_CPPFLAGS += -DHAVE_LIBRETRO
endif
