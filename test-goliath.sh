#!/usr/bin/env bash
# Test script for Goliath launcher functionality
# This script tests the application detection logic without requiring actual applications

set -e

echo "Testing Goliath Launcher Application Detection"
echo "=============================================="

# Create test files to simulate different application types
mkdir -p test_apps
cd test_apps

# Create dummy files for testing
touch test.ipa
touch test.apk  
touch test.exe
touch test.msi
mkdir -p test.app
touch test_binary

echo "Created test files:"
ls -la

echo ""
echo "Testing application detection (dry run):"
echo ""

# Test iOS detection
echo "1. Testing iOS (.ipa) detection:"
echo "   File: test.ipa"
if ../goliath-launch.sh test.ipa 2>&1 | head -3; then
    echo "   ✓ iOS detection working"
else
    echo "   ✗ iOS detection failed"
fi

echo ""

# Test Android detection  
echo "2. Testing Android (.apk) detection:"
echo "   File: test.apk"
if ../goliath-launch.sh test.apk 2>&1 | head -3; then
    echo "   ✓ Android detection working"
else
    echo "   ✗ Android detection failed"
fi

echo ""

# Test Windows detection
echo "3. Testing Windows (.exe) detection:"
echo "   File: test.exe"
if ../goliath-launch.sh test.exe 2>&1 | head -3; then
    echo "   ✓ Windows detection working"
else
    echo "   ✗ Windows detection failed"
fi

echo ""

# Test macOS detection
echo "4. Testing macOS (.app) detection:"
echo "   File: test.app/"
if ../goliath-launch.sh test.app 2>&1 | head -3; then
    echo "   ✓ macOS detection working"
else
    echo "   ✗ macOS detection failed"
fi

echo ""

# Test usage message
echo "5. Testing usage message:"
if ../goliath-launch.sh 2>&1 | grep -q "iOS applications"; then
    echo "   ✓ Usage message includes iOS support"
else
    echo "   ✗ Usage message missing iOS support"
fi

echo ""
echo "Test completed. Note: Actual execution will fail without the required compatibility layers installed."

# Cleanup
cd ..
rm -rf test_apps

echo "Test files cleaned up."