EXTRADEFS = -DXAPOFX1_VER=1 -DXAUDIO2_VER=2
MODULE    = xapofx1_1.dll
IMPORTS   = $(FAUDIO_PE_LIBS) ole32
EXTRAINCL = $(FAUDIO_PE_CFLAGS)
PARENTSRC = ../xaudio2_7

EXTRADLLFLAGS = -Wb,--prefer-native

SOURCES = \
	version.rc \
	xapo.c \
	xapofx.c \
	xaudio_allocator.c \
	xaudio_classes.idl
