noinst_LTLIBRARIES = libgoliath_compatibility.la

libgoliath_compatibility_la_SOURCES = \
        compat_layer.c \
        compat_loader.c \
        compat_mapping.c \
        compat_registry.c

noinst_HEADERS = \
        compat_layer.h \
        compat_loader.h \
        compat_mapping.h \
        compat_registry.h

AM_CFLAGS = -I$(top_srcdir)/include
AM_CPPFLAGS = -I$(top_srcdir)/include

if ENABLE_WINE
AM_CFLAGS += $(WINE_DEPS_CFLAGS)
AM_CPPFLAGS += $(WINE_DEPS_CFLAGS)
endif

if ENABLE_DARLING
AM_CFLAGS += $(DARLING_DEPS_CFLAGS)
AM_CPPFLAGS += $(DARLING_DEPS_CFLAGS)
endif

if ENABLE_ATL
AM_CFLAGS += $(ATL_DEPS_CFLAGS)
AM_CPPFLAGS += $(ATL_DEPS_CFLAGS)
endif
