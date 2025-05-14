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

# Helper: detect file type
filetype=$(file -b "$APP")

# Dispatch logic
if [[ "$filetype" == *"PE32"* || "$filetype" == *"MS Windows"* ]]; then
    # Windows binary
    exec wine "$APP" "$@"
elif [[ "$filetype" == *"Mach-O"* ]]; then
    # macOS binary
    exec darling shell "$APP" "$@"
elif [[ "$filetype" == *"ELF"* && "$APP" == *.apk ]]; then
    # Android APK (very basic check)
    exec atl "$APP" "$@"
else
    echo "[Goliath] Unknown or unsupported application type: $filetype"
    exit 2
fi
