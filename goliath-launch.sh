#!/usr/bin/env bash
# Goliath Unified Compatibility Layer
#
# This is a meta-launcher and integration point for Wine (Windows), Darling (macOS), ATL (Android), and ipasim (iOS).
#
# - Place this script at the project root.
# - Ensure Wine, Darling, ATL, and ipasim are installed and available in PATH.
# - Usage: ./goliath-launch.sh <application> [args...]
#
# This script will auto-detect the application type and dispatch to the correct subsystem.
#
# For more details, see documentation/README-goliath.md

# Goliath Unified Application Launcher
# This script dispatches to Wine, Darling, ATL, or ipasim based on the application type.
# Copyright 2025 Goliath Project

set -e

usage() {
    echo "Usage: $0 <application> [args...]"
    echo "  Runs Windows, macOS, Android, or iOS applications using the appropriate compatibility layer."
    echo ""
    echo "Supported application types:"
    echo "  - Windows executables (.exe, .msi) -> Wine"
    echo "  - macOS applications (.app, Mach-O binaries) -> Darling"
    echo "  - Android packages (.apk) -> ATL"
    echo "  - iOS applications (.ipa) -> ipasim"
    exit 1
}

check_dependency() {
    local cmd="$1"
    local name="$2"
    if ! command -v "$cmd" &> /dev/null; then
        echo "Error: $name is not installed or not in PATH."
        echo "Please install $name and ensure it's available in your PATH."
        exit 1
    fi
}

detect_and_run() {
    local app="$1"
    shift
    
    # Check if file exists
    if [ ! -e "$app" ]; then
        echo "Error: File '$app' does not exist."
        exit 1
    fi
    
    # Get file type information
    local filetype
    filetype=$(file "$app" 2>/dev/null || echo "unknown")
    
    # Detect application type and dispatch to appropriate subsystem
    case "$app" in
        *.ipa)
            echo "Detected iOS application: $app"
            check_dependency "ipasim" "ipasim"
            echo "Launching with ipasim..."
            exec ipasim "$app" "$@"
            ;;
        *.apk)
            echo "Detected Android application: $app"
            check_dependency "atl" "ATL (Android Translation Layer)"
            echo "Launching with ATL..."
            exec atl "$app" "$@"
            ;;
        *.app|*.app/)
            echo "Detected macOS application: $app"
            check_dependency "darling" "Darling"
            echo "Launching with Darling..."
            exec darling shell "$app" "$@"
            ;;
        *.exe|*.msi)
            echo "Detected Windows application: $app"
            check_dependency "wine" "Wine"
            echo "Launching with Wine..."
            exec wine "$app" "$@"
            ;;
        *)
            # Use file command output for more sophisticated detection
            if [[ "$filetype" == *"Mach-O"* ]]; then
                echo "Detected macOS binary (Mach-O): $app"
                check_dependency "darling" "Darling"
                echo "Launching with Darling..."
                exec darling shell "$app" "$@"
            elif [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]]; then
                echo "Detected Windows executable: $app"
                check_dependency "wine" "Wine"
                echo "Launching with Wine..."
                exec wine "$app" "$@"
            elif [[ "$filetype" == *"ELF"* ]]; then
                echo "Detected ELF binary: $app"
                echo "Note: ELF binaries may be Linux native applications."
                echo "Attempting to run natively..."
                exec "$app" "$@"
            else
                echo "Error: Unable to detect application type for '$app'"
                echo "File type: $filetype"
                echo ""
                echo "Supported formats:"
                echo "  - iOS: .ipa files"
                echo "  - Android: .apk files"
                echo "  - macOS: .app bundles, Mach-O binaries"
                echo "  - Windows: .exe, .msi files, PE32 binaries"
                exit 1
            fi
            ;;
    esac
}

if [ $# -lt 1 ]; then
    usage
fi

APP="$1"
shift

# Main execution
detect_and_run "$APP" "$@"
