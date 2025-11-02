
AUTOMAKE_OPTIONS = foreign

noinst_LTLIBRARIES = libgoliath_libretro.la

libgoliath_libretro_la_SOURCES = \
	libretro_loader.c

noinst_HEADERS = \
	libretro_loader.h

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs/libretro_core
AM_CFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libs/libretro_core

libgoliath_libretro_la_LDFLAGS = -shared -fPIC

EXTRA_DIST =
