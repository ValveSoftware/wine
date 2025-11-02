libwsl_la_SOURCES = wsl.c
libwsl_la_CPPFLAGS = -I$(top_srcdir)/include
libwsl_la_LDFLAGS = -shared -fPIC

# Install WSL headers
wslincludedir = $(includedir)/goliath
wslinclude_HEADERS = $(top_srcdir)/include/wsl.h

# Create WSL shared library
lib_LTLIBRARIES = libwsl.la

# WSL configuration files
wslconfdir = $(sysconfdir)/goliath
dist_wslconf_DATA = wsl.conf.example

EXTRA_DIST = wsl.conf.example
