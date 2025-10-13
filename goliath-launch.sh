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

# Run main function with all arguments
main "$@"
