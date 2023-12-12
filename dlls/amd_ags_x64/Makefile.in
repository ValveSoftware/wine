EXTRADEFS = -DWINE_NO_LONG_TYPES
MODULE    = amd_ags_x64.dll
UNIXLIB   = amd_ags_x64.so
UNIX_CFLAGS = $(DRM_CFLAGS)
UNIX_LIBS = $(DRM_LIBS) $(DRMAMDGPU_LIBS)
IMPORTS   = version vulkan-1 user32
IMPORTLIB = amd_ags_x64

EXTRADLLFLAGS = -mno-cygwin

SOURCES = \
	amd_ags_x64_main.c \
	unixlib.c \
	dxvk_interfaces.idl
