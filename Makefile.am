SUBDIRS = \
	tools \
	dlls \
	programs \
	libs \
	loader \
	server \
	fonts \
	include \
	documentation \
	libs/atl_android \
	libs/darling

EXTRA_DIST = goliath-launch.sh

if USE_DARLING
SUBDIRS += libs/darling
endif

if USE_ATL
SUBDIRS += libs/atl-tmp
endif
