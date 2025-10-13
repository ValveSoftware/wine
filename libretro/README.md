# LibRetro Integration Module for Goliath

This directory contains the LibRetro integration components for the Goliath compatibility layer.

## Structure

- `libretro_core.c/h` - Core management and loading functionality
- `libretro_api.c/h` - LibRetro API wrapper and interface
- `rom_detection.c/h` - ROM file format detection and validation
- `Makefile.in` - Build configuration for LibRetro module

## Purpose

Enables Goliath to run legacy console and hardware applications through LibRetro cores, providing emulation support for systems like NES, SNES, Genesis, and others.