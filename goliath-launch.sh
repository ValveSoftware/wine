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

# Enable debug output if GOLIATH_DEBUG is set
if [ "${GOLIATH_DEBUG:-0}" = "1" ]; then
    set -x
fi

usage() {
    echo "Goliath Unified Compatibility Layer"
    echo "Usage: $0 <application> [args...]"
    echo ""
    echo "Runs Windows, macOS, or Android applications using the appropriate compatibility layer."
    echo ""
    echo "Supported formats:"
    echo "  - Windows PE executables (.exe, .msi, .dll)"
    echo "  - macOS Mach-O binaries and .app bundles"
    echo "  - Android APK files"
    echo ""
    echo "Environment variables:"
    echo "  GOLIATH_DEBUG=1    Enable debug output"
    echo "  GOLIATH_FORCE=wine|darling|atl    Force specific compatibility layer"
    echo ""
    exit 1
}

log_info() {
    echo "[GOLIATH] $*" >&2
}

log_error() {
    echo "[GOLIATH ERROR] $*" >&2
}

# Check if we have at least one argument
if [ $# -lt 1 ]; then
    usage
fi

APP="$1"
shift

# Check if the application exists
if [ ! -e "$APP" ]; then
    log_error "Application not found: $APP"
    exit 1
fi

# Function to detect application type
detect_app_type() {
    local app_path="$1"
    
    # Check if it's a macOS application bundle
    if [ -d "$app_path" ] && [[ "$app_path" == *.app ]]; then
        echo "macos_bundle"
        return 0
    fi
    
    # Check file extension first for quick detection
    case "${app_path,,}" in
        *.exe|*.msi|*.dll)
            echo "windows"
            return 0
            ;;
        *.app)
            echo "macos_bundle"
            return 0
            ;;
        *.apk)
            echo "android"
            return 0
            ;;
    esac
    
    # Use file command for binary detection
    if command -v file >/dev/null 2>&1; then
        local filetype
        filetype=$(file -b "$app_path" 2>/dev/null)
        
        if [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]]; then
            echo "windows"
            return 0
        elif [[ "$filetype" == *"Mach-O"* ]]; then
            echo "macos"
            return 0
        elif [[ "$filetype" == *"ELF"* ]]; then
            # Could be Linux binary or Android native library
            if [[ "$app_path" == *.apk ]]; then
                echo "android"
            else
                echo "linux"
            fi
            return 0
        fi
    fi
    
    # Fallback: try to detect by magic numbers
    if [ -f "$app_path" ] && [ -r "$app_path" ]; then
        local magic
        magic=$(hexdump -C "$app_path" 2>/dev/null | head -1 | cut -d' ' -f2-5 | tr -d ' ')
        
        case "$magic" in
            4d5a*|5a4d*)  # MZ header (Windows PE)
                echo "windows"
                return 0
                ;;
            feedface|feedfacf|cefaedfe|cffaedfe)  # Mach-O magic numbers
                echo "macos"
                return 0
                ;;
            7f454c46*)  # ELF magic
                echo "linux"
                return 0
                ;;
        esac
    fi
    
    echo "unknown"
    return 1
}

# Function to check if a compatibility layer is available
check_compatibility_layer() {
    local layer="$1"
    
    case "$layer" in
        wine)
            command -v wine >/dev/null 2>&1
            ;;
        darling)
            command -v darling >/dev/null 2>&1 || [ -x "$(dirname "$0")/loader/goliath" ]
            ;;
        atl)
            command -v atl >/dev/null 2>&1
            ;;
        *)
            return 1
            ;;
    esac
}

# Function to launch with specific compatibility layer
launch_with_layer() {
    local layer="$1"
    local app_path="$2"
    shift 2
    
    case "$layer" in
        wine)
            log_info "Launching Windows application with Wine: $app_path"
            exec wine "$app_path" "$@"
            ;;
        darling)
            log_info "Launching macOS application with Darling: $app_path"
            # Try unified loader first, then fallback to system darling
            if [ -x "$(dirname "$0")/loader/goliath" ]; then
                exec "$(dirname "$0")/loader/goliath" "$app_path" "$@"
            elif command -v darling >/dev/null 2>&1; then
                exec darling shell "$app_path" "$@"
            else
                log_error "Darling not found. Please install Darling or build Goliath with Darling support."
                exit 1
            fi
            ;;
        atl)
            log_info "Launching Android application with ATL: $app_path"
            if command -v atl >/dev/null 2>&1; then
                exec atl "$app_path" "$@"
            else
                log_error "ATL (Android Translation Layer) not found."
                exit 1
            fi
            ;;
        *)
            log_error "Unknown compatibility layer: $layer"
            exit 1
            ;;
    esac
}

# Handle macOS application bundles
if [ -d "$APP" ] && [[ "$APP" == *.app ]]; then
    # Find the executable inside the bundle
    if [ -f "$APP/Contents/MacOS/"* ]; then
        BUNDLE_EXEC=$(find "$APP/Contents/MacOS" -type f -executable | head -1)
        if [ -n "$BUNDLE_EXEC" ]; then
            log_info "Found macOS application bundle: $APP"
            log_info "Executable: $BUNDLE_EXEC"
            APP="$BUNDLE_EXEC"
        else
            log_error "No executable found in macOS application bundle: $APP"
            exit 1
        fi
    else
        log_error "Invalid macOS application bundle structure: $APP"
        exit 1
    fi
fi

# Check for forced compatibility layer
if [ -n "${GOLIATH_FORCE:-}" ]; then
    log_info "Forcing compatibility layer: $GOLIATH_FORCE"
    if check_compatibility_layer "$GOLIATH_FORCE"; then
        launch_with_layer "$GOLIATH_FORCE" "$APP" "$@"
    else
        log_error "Forced compatibility layer '$GOLIATH_FORCE' is not available"
        exit 1
    fi
fi

# Detect application type
APP_TYPE=$(detect_app_type "$APP")
log_info "Detected application type: $APP_TYPE for $APP"

# Launch with appropriate compatibility layer
case "$APP_TYPE" in
    windows)
        if check_compatibility_layer wine; then
            launch_with_layer wine "$APP" "$@"
        else
            log_error "Wine not found. Please install Wine to run Windows applications."
            exit 1
        fi
        ;;
    macos|macos_bundle)
        if check_compatibility_layer darling; then
            launch_with_layer darling "$APP" "$@"
        else
            log_error "Darling not found. Please install Darling to run macOS applications."
            exit 1
        fi
        ;;
    android)
        if check_compatibility_layer atl; then
            launch_with_layer atl "$APP" "$@"
        else
            log_error "ATL not found. Please install ATL to run Android applications."
            exit 1
        fi
        ;;
    linux)
        log_info "Detected Linux binary, running natively"
        exec "$APP" "$@"
        ;;
    unknown)
        log_error "Unable to determine application type for: $APP"
        log_error "Supported formats: Windows PE, macOS Mach-O, Android APK"
        log_error "Use GOLIATH_FORCE environment variable to override detection"
        exit 1
        ;;
    *)
        log_error "Unsupported application type: $APP_TYPE"
        exit 1
        ;;
esac
