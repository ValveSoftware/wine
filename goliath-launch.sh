#!/usr/bin/env bash
# Goliath Unified Compatibility Layer
#
# This is a meta-launcher and integration point for Wine (Windows), Darling (macOS), 
# ATL (Android), ipasim (iOS), and LibRetro (Legacy ROMs).
#
# - Place this script at the project root.
# - Ensure Wine, Darling, ATL, ipasim, and LibRetro cores are installed and available.
# - Usage: ./goliath-launch.sh <application> [args...]
#
# This script will auto-detect the application type and dispatch to the correct subsystem.
#
# For more details, see documentation/README-goliath.md
#
# Copyright 2025 Goliath Project

set -o pipefail
trap 'echo "ERROR: Goliath launcher failed at line ${LINENO}." >&2; exit 1' ERR

# Enable debug mode if GOLIATH_DEBUG is set
DEBUG=${GOLIATH_DEBUG:-0}

debug_log() {
    if [[ "$DEBUG" == "1" ]]; then
        echo "[DEBUG] $*" >&2
    fi
}

usage() {
    echo "Usage: $0 <application> [args...]"
    echo "  Runs Windows, macOS, Android, iOS, or ROM applications using the appropriate compatibility layer."
    echo ""
    echo "Supported application types:"
    echo "  - Windows executables (.exe, .msi) -> Wine"
    echo "  - macOS applications (.app, Mach-O binaries) -> Darling"
    echo "  - Android packages (.apk) -> ATL"
    echo "  - iOS applications (.ipa) -> ipasim"
    echo "  - ROM files (various formats) -> LibRetro"
    echo ""
    echo "Environment variables:"
    echo "  GOLIATH_DEBUG=1    Enable debug output"
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

# Detect file type based on extension and file magic
detect_file_type() {
    local app="$1"
    
    # Check if file exists
    if [[ ! -e "$app" ]]; then
        echo "unknown"
        return
    fi
    
    # Get file type information
    local filetype
    filetype=$(file "$app" 2>/dev/null || echo "unknown")
    
    # Check by extension first
    case "$app" in
        *.ipa)
            echo "ios"
            return
            ;;
        *.apk)
            echo "android"
            return
            ;;
        *.app|*.app/)
            echo "macos"
            return
            ;;
        *.exe|*.msi)
            echo "windows"
            return
            ;;
        *.nes|*.smc|*.sfc|*.gb|*.gbc|*.gba|*.md|*.gen|*.32x|*.gg|*.ms|*.pce|*.ngp|*.ngc|*.ws|*.wsc|*.vb|*.rom|*.bin)
            echo "rom_$(get_rom_type "$app")"
            return
            ;;
    esac
    
    # Use file command output for more sophisticated detection
    if [[ "$filetype" == *"Mach-O"* ]]; then
        echo "macos"
    elif [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]]; then
        echo "windows"
    elif [[ "$filetype" == *"ELF"* ]]; then
        echo "linux"
    else
        # Try to detect ROM files by content
        local rom_type
        rom_type=$(get_rom_type "$app")
        if [[ "$rom_type" != "unknown" ]]; then
            echo "rom_$rom_type"
        else
            echo "unknown"
        fi
    fi
}

# Detect ROM type for LibRetro
get_rom_type() {
    local rom="$1"
    local ext="${rom##*.}"
    
    case "${ext,,}" in
        nes) echo "nes" ;;
        smc|sfc) echo "snes" ;;
        gb) echo "gameboy" ;;
        gbc) echo "gameboy_color" ;;
        gba) echo "gameboy_advance" ;;
        md|gen) echo "genesis" ;;
        32x) echo "sega32x" ;;
        gg) echo "gamegear" ;;
        ms) echo "mastersystem" ;;
        pce) echo "pcengine" ;;
        ngp|ngc) echo "neogeo_pocket" ;;
        ws|wsc) echo "wonderswan" ;;
        vb) echo "virtualboy" ;;
        *) echo "unknown" ;;
    esac
}

# Run Windows applications via Wine
run_windows_app() {
    local app="$1"
    shift
    
    debug_log "Running Windows application: $app"
    check_dependency "wine" "Wine"
    echo "Launching with Wine..."
    exec wine "$app" "$@"
}

# Run macOS applications via Darling
run_macos_app() {
    local app="$1"
    shift
    
    debug_log "Running macOS application: $app"
    check_dependency "darling" "Darling"
    echo "Launching with Darling..."
    exec darling shell "$app" "$@"
}

# Run Android applications via ATL
run_android_app() {
    local app="$1"
    shift
    
    debug_log "Running Android application: $app"
    check_dependency "atl" "ATL (Android Translation Layer)"
    echo "Launching with ATL..."
    exec atl "$app" "$@"
}

# Run iOS applications via ipasim
run_ios_app() {
    local app="$1"
    shift
    
    debug_log "Running iOS application: $app"
    check_dependency "ipasim" "ipasim"
    echo "Launching with ipasim..."
    exec ipasim "$app" "$@"
}

# Run ROM files via LibRetro
run_rom_file() {
    local rom="$1"
    shift
    
    debug_log "Running ROM file: $rom"
    
    # Use the unified loader which has LibRetro integration
    local loader="$(dirname "$0")/loader/goliath"
    
    if [[ ! -x "$loader" ]]; then
        # Try to build the loader if it doesn't exist
        echo "Goliath unified loader not found. Attempting to build..." >&2
        if [[ -f "$(dirname "$0")/Makefile" ]]; then
            make -C "$(dirname "$0")" loader/goliath
        fi
        
        if [[ ! -x "$loader" ]]; then
            echo "ERROR: Goliath unified loader not available." >&2
            echo "Please build the project first: make" >&2
            return 1
        fi
    fi
    
    exec "$loader" "$rom" "$@"
}

# Run Linux native applications
run_linux_app() {
    local app="$1"
    shift
    
    debug_log "Running Linux application: $app"
    echo "Running native Linux application..."
    exec "$app" "$@"
}

# Main function - unified entry point
main() {
    if [[ $# -lt 1 ]]; then
        usage
    fi
    
    local app="$1"
    shift
    
    # Check if file exists
    if [[ ! -e "$app" ]]; then
        echo "Error: File '$app' does not exist." >&2
        exit 1
    fi
    
    # Make path absolute if relative
    if [[ "$app" != /* ]]; then
        app="$(pwd)/$app"
    fi
    
    debug_log "Goliath launcher starting..."
    debug_log "Target application: $app"
    debug_log "Arguments: $*"
    
    # Detect file type
    local file_type
    file_type=$(detect_file_type "$app")
    
    debug_log "Detected file type: $file_type"
    
    case "$file_type" in
        windows)
            run_windows_app "$app" "$@"
            ;;
        macos)
            run_macos_app "$app" "$@"
            ;;
        android)
            run_android_app "$app" "$@"
            ;;
        ios)
            run_ios_app "$app" "$@"
            ;;
        linux)
            run_linux_app "$app" "$@"
            ;;
        rom_*)
            run_rom_file "$app" "$@"
            ;;
        unknown)
            echo "ERROR: Unable to detect application type for '$app'" >&2
            local filetype
            filetype=$(file "$app" 2>/dev/null || echo "unknown")
            echo "File type: $filetype" >&2
            echo "" >&2
            echo "Supported formats:" >&2
            echo "  - iOS: .ipa files" >&2
            echo "  - Android: .apk files" >&2
            echo "  - macOS: .app bundles, Mach-O binaries" >&2
            echo "  - Windows: .exe, .msi files, PE32 binaries" >&2
            echo "  - ROMs: .nes, .smc, .gb, .gba, .md, etc." >&2
            exit 1
            ;;
        *)
            echo "ERROR: Unsupported file type: $file_type" >&2
            echo "File: $app" >&2
            usage
            ;;
    esac
}

# Run main function with all arguments
main "$@"
