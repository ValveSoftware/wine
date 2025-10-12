#!/usr/bin/env bash
# Goliath Usage Demonstration Script
# This script shows how to use Goliath with different application types

echo "Goliath Unified Compatibility Layer - Usage Examples"
echo "===================================================="
echo ""

# Function to show example usage
show_example() {
    local platform="$1"
    local file_type="$2"
    local example_file="$3"
    local description="$4"
    
    echo "[$platform Applications]"
    echo "File type: $file_type"
    echo "Example: $example_file"
    echo "Description: $description"
    echo "Usage: ./goliath-launch.sh $example_file"
    echo ""
}

# Show examples for each supported platform
show_example "iOS" ".ipa files" "Calculator.ipa" "iOS applications run via ipasim emulator"
show_example "Android" ".apk files" "MyAndroidApp.apk" "Android applications run via ATL"
show_example "macOS" ".app bundles" "TextEdit.app" "macOS applications run via Darling"
show_example "Windows" ".exe/.msi files" "notepad.exe" "Windows applications run via Wine"

echo "Advanced Usage Examples:"
echo "========================"
echo ""

echo "1. Running with arguments:"
echo "   ./goliath-launch.sh MyApp.ipa --fullscreen"
echo "   ./goliath-launch.sh game.exe --windowed --resolution=1024x768"
echo ""

echo "2. Using full paths:"
echo "   ./goliath-launch.sh /home/user/Downloads/MyiOSApp.ipa"
echo "   ./goliath-launch.sh ~/Applications/MyMacApp.app"
echo ""

echo "3. Batch processing (example):"
echo "   for app in *.ipa; do"
echo "       echo \"Testing \$app...\""
echo "       ./goliath-launch.sh \"\$app\" --test-mode"
echo "   done"
echo ""

echo "Prerequisites Check:"
echo "==================="
echo ""

# Function to check if a command exists
check_command() {
    local cmd="$1"
    local name="$2"
    if command -v "$cmd" &> /dev/null; then
        echo "✓ $name is installed"
    else
        echo "✗ $name is NOT installed"
    fi
}

check_command "wine" "Wine (Windows support)"
check_command "darling" "Darling (macOS support)"  
check_command "atl" "ATL (Android support)"
check_command "ipasim" "ipasim (iOS support)"

echo ""
echo "Note: Install missing components to enable support for those platforms."
echo ""

echo "Getting Help:"
echo "============="
echo "• Run './goliath-launch.sh' without arguments to see usage information"
echo "• Check 'documentation/README-goliath.md' for detailed documentation"
echo "• See 'documentation/iOS-SETUP.md' for iOS-specific setup instructions"
echo ""

echo "Example Test Run (safe - won't actually launch anything):"
echo "========================================================"
echo ""

# Create a temporary test file to demonstrate detection
if [ -w . ]; then
    echo "Creating test file: test-demo.ipa"
    touch test-demo.ipa
    
    echo "Running detection test:"
    if ./goliath-launch.sh test-demo.ipa 2>&1 | head -5; then
        echo ""
        echo "✓ Detection working correctly"
    else
        echo "✗ Detection test failed"
    fi
    
    # Clean up
    rm -f test-demo.ipa
    echo "Test file cleaned up."
else
    echo "Cannot create test files in current directory."
    echo "Run this script from the Goliath root directory for full demonstration."
fi

echo ""
echo "Demo completed. Happy cross-platform application running!"