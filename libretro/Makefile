# LibRetro Integration Module Makefile
# Copyright 2025 Goliath Project

SRCDIR = .


CC = gcc
CFLAGS = -g -O2 -fPIC
LDFLAGS = 
LIBS = -ldl  -ldl

# Source files
SOURCES = \
	libretro_api.c \
	rom_detection.c

# Object files
OBJECTS = $(SOURCES:.c=.o)

# Target library
TARGET = libgoliath_libretro.a

# Default target
all: $(TARGET)

# Build the library
$(TARGET): $(OBJECTS)
	$(AR) rcs $@ $(OBJECTS)

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Dependencies
libretro_api.o: libretro_api.c libretro_api.h rom_detection.h
rom_detection.o: rom_detection.c rom_detection.h

# Clean target
clean:
	rm -f $(OBJECTS) $(TARGET)

# Install target
install: $(TARGET)
	mkdir -p $(DESTDIR)$(libdir)
	cp $(TARGET) $(DESTDIR)$(libdir)/

# Uninstall target
uninstall:
	rm -f $(DESTDIR)$(libdir)/$(TARGET)

.PHONY: all clean install uninstall
