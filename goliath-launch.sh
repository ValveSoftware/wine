#!/usr/bin/env bash
# Goliath Unified Compatibility Layer
#
# This is a meta-launcher and integration point for Wine (Windows), Darling (macOS), WSL (Linux), and ATL (Android).
#
# - Place this script at the project root.
# - Ensure Wine, Darling, WSL, and ATL are installed and available in PATH.
# - Usage: ./goliath-launch.sh <application> [args...]
#
# This script will auto-detect the application type and dispatch to the correct subsystem.
#
# For more details, see documentation/README-goliath.md

# Goliath Unified Application Launcher
# This script dispatches to Wine, Darling, WSL, or ATL based on the application type.
# Copyright 2025 Goliath Project

set -e

usage() {
    echo "Usage: $0 <application> [args...]"
    echo "  Runs Windows, macOS, Linux, or Android applications using the appropriate compatibility layer."
    echo ""
    echo "Supported formats:"
    echo "  - Windows PE/EXE files (via Wine)"
    echo "  - macOS Mach-O binaries (via Darling)"
    echo "  - Linux ELF binaries (via WSL)"
    echo "  - Android APK files (via ATL)"
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

APP="$1"
shift

# Check if file exists
if [ ! -f "$APP" ]; then
    echo "Error: File '$APP' not found"
    exit 1
fi

# Detect file type using file command and magic numbers
detect_app_type() {
    local app="$1"
    
    # Get file type information
    local filetype=$(file "$app" 2>/dev/null)
    
    # Check for Windows PE executables
    if [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]] || [[ "$app" == *.exe ]] || [[ "$app" == *.msi ]]; then
        echo "windows"
        return
    fi
    
    # Check for macOS Mach-O binaries
    if [[ "$filetype" == *"Mach-O"* ]] || [[ "$app" == *.app ]] || [[ "$app" == *.dmg ]]; then
        echo "macos"
        return
    fi
    
    # Check for Android APK files
    if [[ "$filetype" == *"Zip archive"* && "$app" == *.apk ]] || [[ "$app" == *.apk ]]; then
        echo "android"
        return
    fi
    
    # Check for Linux ELF binaries
    if [[ "$filetype" == *"ELF"* ]]; then
        echo "linux"
        return
    fi
    
    # Default to unknown
    echo "unknown"
}

APP_TYPE=$(detect_app_type "$APP")

case "$APP_TYPE" in
    "windows")
        echo "[Goliath] Detected Windows application, launching via Wine..."
        if command -v wine >/dev/null 2>&1; then
            exec wine "$APP" "$@"
        else
            # Use the unified Goliath loader
            exec "$(dirname "$0")/loader/wine" "$APP" "$@"
        fi
        ;;
    "macos")
        echo "[Goliath] Detected macOS application, launching via Darling..."
        if command -v darling >/dev/null 2>&1; then
            exec darling shell "$APP" "$@"
        else
            # Use the unified Goliath loader
            exec "$(dirname "$0")/loader/goliath" "$APP" "$@"
        fi
        ;;
    "linux")
        echo "[Goliath] Detected Linux application, launching via WSL..."
        # Use the unified Goliath loader for WSL
        exec "$(dirname "$0")/loader/goliath" "$APP" "$@"
        ;;
    "android")
        echo "[Goliath] Detected Android application, launching via ATL..."
        if command -v atl >/dev/null 2>&1; then
            exec atl "$APP" "$@"
        else
            # Use the unified Goliath loader
            exec "$(dirname "$0")/loader/goliath" "$APP" "$@"
        fi
        ;;
    *)
        echo "Error: Unknown or unsupported application format: $APP"
        echo "File type: $(file "$APP" 2>/dev/null || echo "Unable to determine")"
        exit 2
        ;;
esac
