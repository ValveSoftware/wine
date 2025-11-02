ACLOCAL_AMFLAGS = -I m4

SUBDIRS = \
        tools \
        loader \
        include \
        documentation

# Base libraries
SUBDIRS += \
        libs/goliath_core \
        libs/common \
        libs/compatibility

# Wine components (if enabled)
if ENABLE_WINE
SUBDIRS += \
        upstream/wine
endif

# Darling components (if enabled)
if ENABLE_DARLING
SUBDIRS += \
        dlls/darling_coreaudio \
        dlls/darling_sandbox \
        libs/darling \
        libs/darling_framework
endif

# Android Translation Layer components (if enabled)
if ENABLE_ATL
SUBDIRS += \
        libs/atl_android \
        libs/atl_runtime \
        dlls/atl_bridge
endif

# WSL components (if enabled)
if ENABLE_WSL
SUBDIRS += \
        dlls/wsl \
        libs/wsl_core \
        libs/wsl_bridge
endif

# LibRetro components (if enabled)
if ENABLE_LIBRETRO
SUBDIRS += \
        libretro \
        libs/libretro_core
endif

EXTRA_DIST = \
        goliath-launch.sh \
        README.md \
        CHANGELOG.md \
        LICENSE
