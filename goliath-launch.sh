#!/usr/bin/env bash
# Goliath Unified Compatibility Layer
#
# This is a meta-launcher and integration point for Wine (Windows), Darling (macOS), and ATL (Android).
#
# - Place this script at the project root.
# - Ensure Wine, Darling, and ATL are installed and available in PATH.
# - Usage: ./goliath-launch.sh <application> [args...]
#
# This script will auto-detect the application type and dispatch to the correct subsystem.
#
# For more details, see documentation/README-goliath.md

# Goliath Unified Application Launcher
# This script dispatches to Wine, Darling, or ATL based on the application type.
# Copyright 2025 Goliath Project

set -e

usage() {
    echo "Usage: $0 <application> [args...]"
    echo "  Runs Windows, macOS, or Android applications using the appropriate compatibility layer."
    exit 1
}

if [ $# -lt 1 ]; then
    usage
fi

APP="$1"
shift

# Detect application type
detect_app_type() {
    local app="$1"
    
    # Check if file exists
    if [ ! -f "$app" ]; then
        echo "Error: File '$app' not found" >&2
        exit 1
    fi
    
    # Get file type information
    local filetype=$(file -b "$app" 2>/dev/null)
    local extension="${app##*.}"
    
    # Android APK detection
    if [[ "$extension" == "apk" ]] || [[ "$filetype" == *"Zip archive"* && "$app" == *.apk ]]; then
        # Verify it's actually an APK by checking for AndroidManifest.xml
        if command -v unzip >/dev/null 2>&1 && unzip -l "$app" 2>/dev/null | grep -q "AndroidManifest.xml"; then
            echo "android"
            return
        fi
    fi
    
    # macOS application detection
    if [[ "$filetype" == *"Mach-O"* ]] || [[ "$extension" == "app" ]] || [[ "$app" == *.app/* ]]; then
        echo "macos"
        return
    fi
    
    # Windows executable detection
    if [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]] || [[ "$extension" == "exe" ]] || [[ "$extension" == "msi" ]]; then
        echo "windows"
        return
    fi
    
    # Default to windows for unknown types (Wine can handle many formats)
    echo "windows"
}

# Check for required tools
check_dependencies() {
    local app_type="$1"
    
    case "$app_type" in
        "android")
            if ! command -v atl >/dev/null 2>&1; then
                echo "Error: ATL (Android Translation Layer) not found in PATH" >&2
                echo "Please install ATL from: https://gitlab.com/android_translation_layer/android_translation_layer" >&2
                exit 1
            fi
            ;;
        "macos")
            if ! command -v darling >/dev/null 2>&1; then
                echo "Error: Darling not found in PATH" >&2
                echo "Please install Darling from: https://github.com/darlinghq/darling" >&2
                exit 1
            fi
            ;;
        "windows")
            # Wine should be available as part of Goliath build
            if [ ! -f "$(dirname "$0")/wine" ] && ! command -v wine >/dev/null 2>&1; then
                echo "Error: Wine not found" >&2
                echo "Please build Goliath or install Wine" >&2
                exit 1
            fi
            ;;
    esac
}

# Detect the application type
APP_TYPE=$(detect_app_type "$APP")
echo "Detected application type: $APP_TYPE"

# Check dependencies
check_dependencies "$APP_TYPE"

# Launch the application with the appropriate compatibility layer
case "$APP_TYPE" in
    "android")
        echo "Launching Android app with ATL..."
        exec atl "$APP" "$@"
        ;;
    "macos")
        echo "Launching macOS app with Darling..."
        exec darling "$APP" "$@"
        ;;
    "windows")
        echo "Launching Windows app with Wine..."
        # Try to use the built Goliath wine first, then system wine
        if [ -f "$(dirname "$0")/wine" ]; then
            exec "$(dirname "$0")/wine" "$APP" "$@"
        else
            exec wine "$APP" "$@"
        fi
        ;;
    *)
        echo "Error: Unsupported application type: $APP_TYPE" >&2
        exit 1
        ;;
esac
