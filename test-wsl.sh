#!/usr/bin/env bash
# Test script for Goliath WSL functionality
# This script tests the WSL compatibility layer

set -e

echo "=== Goliath WSL Test Suite ==="
echo

# Test 1: Check if goliath-launch.sh exists and is executable
echo "Test 1: Checking goliath-launch.sh..."
if [ -f "./goliath-launch.sh" ]; then
    echo "✓ goliath-launch.sh found"
    chmod +x ./goliath-launch.sh
    echo "✓ Made goliath-launch.sh executable"
else
    echo "✗ goliath-launch.sh not found"
    exit 1
fi

# Test 2: Test basic functionality with /bin/echo (if available)
echo
echo "Test 2: Testing WSL with /bin/echo..."
if [ -f "/bin/echo" ]; then
    echo "Running: ./goliath-launch.sh /bin/echo 'Hello from Goliath WSL!'"
    ./goliath-launch.sh /bin/echo "Hello from Goliath WSL!"
    echo "✓ WSL basic test passed"
else
    echo "⚠ /bin/echo not found, skipping test"
fi

# Test 3: Test with /usr/bin/whoami (if available)
echo
echo "Test 3: Testing WSL with /usr/bin/whoami..."
if [ -f "/usr/bin/whoami" ]; then
    echo "Running: ./goliath-launch.sh /usr/bin/whoami"
    ./goliath-launch.sh /usr/bin/whoami
    echo "✓ WSL whoami test passed"
else
    echo "⚠ /usr/bin/whoami not found, skipping test"
fi

# Test 4: Test with /bin/ls (if available)
echo
echo "Test 4: Testing WSL with /bin/ls..."
if [ -f "/bin/ls" ]; then
    echo "Running: ./goliath-launch.sh /bin/ls -la /tmp"
    ./goliath-launch.sh /bin/ls -la /tmp
    echo "✓ WSL ls test passed"
else
    echo "⚠ /bin/ls not found, skipping test"
fi

# Test 5: Test file type detection
echo
echo "Test 5: Testing file type detection..."
echo "Creating test files..."

# Create a simple ELF test (copy of /bin/echo if available)
if [ -f "/bin/echo" ]; then
    cp /bin/echo ./test_elf_binary
    echo "Testing ELF detection:"
    file ./test_elf_binary
    ./goliath-launch.sh ./test_elf_binary "ELF test successful"
    rm -f ./test_elf_binary
    echo "✓ ELF detection test passed"
fi

echo
echo "=== All WSL tests completed ==="
echo "Goliath WSL compatibility layer is working!"