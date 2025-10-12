#!/usr/bin/env bash
# Goliath Unified Compatibility Layer
#
# This is a meta-launcher and integration point for Wine (Windows), Darling (macOS), 
# ATL (Android), and LibRetro (Legacy ROMs).
#
# - Place this script at the project root.
# - Ensure Wine, Darling, ATL, and LibRetro cores are installed and available.
# - Usage: ./goliath-launch.sh <application> [args...]
#
# This script will auto-detect the application type and dispatch to the correct subsystem.
#
# For more details, see documentation/README-goliath.md

# Goliath Unified Application Launcher
# This script dispatches to Wine, Darling, ATL, or LibRetro based on the application type.
# Copyright 2025 Goliath Project

set -e

usage() {
    echo "Usage: $0 <application> [args...]"
    echo "  Runs Windows, macOS, Android, or Legacy ROM applications using the appropriate compatibility layer."
    echo ""
    echo "Supported file types:"
    echo "  - Windows executables (.exe, .msi, .bat)"
    echo "  - macOS applications (.app, .dmg, Mach-O binaries)"
    echo "  - Android packages (.apk)"
    echo "  - Legacy ROMs (.nes, .snes, .smc, .md, .gen, .gb, .gbc, .gba, .n64, .z64, etc.)"
    echo ""
    echo "Environment variables:"
    echo "  GOLIATH_CORES_PATH - Path to LibRetro cores directory"
    echo "  GOLIATH_WINE_PATH  - Path to Wine installation"
    echo "  GOLIATH_DEBUG      - Enable debug output (1=enabled)"
    exit 1
}

debug_log() {
    if [[ "${GOLIATH_DEBUG:-0}" == "1" ]]; then
        echo "[DEBUG] $*" >&2
    fi
}

detect_file_type() {
    local file="$1"
    
    if [[ ! -f "$file" ]]; then
        echo "ERROR: File not found: $file" >&2
        return 1
    fi
    
    # Get file extension
    local ext="${file##*.}"
    ext=$(echo "$ext" | tr '[:upper:]' '[:lower:]')
    
    debug_log "File: $file, Extension: $ext"
    
    # Check for ROM files first (by extension)
    case "$ext" in
        nes|unf|unif|fds)
            echo "rom_nes"
            return 0
            ;;
        smc|sfc|fig)
            echo "rom_snes"
            return 0
            ;;
        md|gen|bin|smd)
            # Check if it's a Genesis ROM by looking for SEGA signature
            if command -v hexdump >/dev/null 2>&1; then
                local header=$(hexdump -C "$file" | head -20 | grep -i "sega")
                if [[ -n "$header" ]]; then
                    echo "rom_genesis"
                    return 0
                fi
            fi
            # Could also be a generic binary, check further
            ;;
        gb)
            echo "rom_gameboy"
            return 0
            ;;
        gbc)
            echo "rom_gameboy_color"
            return 0
            ;;
        gba)
            echo "rom_gameboy_advance"
            return 0
            ;;
        n64|v64|z64)
            echo "rom_n64"
            return 0
            ;;
        sms)
            echo "rom_master_system"
            return 0
            ;;
        gg)
            echo "rom_game_gear"
            return 0
            ;;
        a26)
            echo "rom_atari_2600"
            return 0
            ;;
        a78)
            echo "rom_atari_7800"
            return 0
            ;;
        lnx)
            echo "rom_lynx"
            return 0
            ;;
        apk)
            echo "android"
            return 0
            ;;
        exe|msi|bat|com)
            echo "windows"
            return 0
            ;;
        app|dmg)
            echo "macos"
            return 0
            ;;
    esac
    
    # Use file command to detect binary type
    if command -v file >/dev/null 2>&1; then
        local filetype=$(file "$file")
        debug_log "File type: $filetype"
        
        if [[ "$filetype" == *"Mach-O"* ]]; then
            echo "macos"
            return 0
        elif [[ "$filetype" == *"ELF"* ]]; then
            # Could be a Linux binary or Windows binary in ELF format
            if [[ "$filetype" == *"Windows"* ]] || [[ "$ext" == "exe" ]]; then
                echo "windows"
            else
                echo "linux"
            fi
            return 0
        elif [[ "$filetype" == *"PE32"* ]] || [[ "$filetype" == *"MS-DOS"* ]]; then
            echo "windows"
            return 0
        fi
    fi
    
    # If we can't determine the type, try the unified loader
    echo "unknown"
    return 0
}

run_windows_app() {
    local app="$1"
    shift
    
    debug_log "Running Windows application: $app"
    
    # Check if Wine is available
    if ! command -v wine >/dev/null 2>&1; then
        echo "ERROR: Wine not found. Please install Wine to run Windows applications." >&2
        return 1
    fi
    
    exec wine "$app" "$@"
}

run_macos_app() {
    local app="$1"
    shift
    
    debug_log "Running macOS application: $app"
    
    # Check if Darling is available
    if ! command -v darling >/dev/null 2>&1; then
        echo "ERROR: Darling not found. Please install Darling to run macOS applications." >&2
        return 1
    fi
    
    exec darling shell "$app" "$@"
}

run_android_app() {
    local app="$1"
    shift
    
    debug_log "Running Android application: $app"
    
    # Check if ATL is available
    if ! command -v atl >/dev/null 2>&1; then
        echo "ERROR: ATL (Android Translation Layer) not found." >&2
        echo "Please install ATL to run Android applications." >&2
        return 1
    fi
    
    exec atl "$app" "$@"
}

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

run_linux_app() {
    local app="$1"
    shift
    
    debug_log "Running Linux application: $app"
    
    # For Linux binaries, just execute directly
    exec "$app" "$@"
}

main() {
    if [[ $# -lt 1 ]]; then
        usage
    fi
    
    local app="$1"
    shift
    
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
        linux)
            run_linux_app "$app" "$@"
            ;;
        rom_*)
            run_rom_file "$app" "$@"
            ;;
        unknown)
            echo "WARNING: Unknown file type, trying unified loader..." >&2
            run_rom_file "$app" "$@"  # The unified loader can handle unknown types
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
