#!/bin/bash

# Compilation script for uthreads library
# Make sure to have proper permissions: chmod +x compile.sh

echo "Compiling uthreads library..."

# Define compilation flags
CFLAGS="-std=c17 -Wall -Wextra -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L"

# Compile uthreads.c to object file
echo "Step 1: Compiling uthreads.c..."
gcc $CFLAGS -c uthreads.c -o uthreads.o

if [ $? -ne 0 ]; then
    echo "❌ ERROR: Failed to compile uthreads.c"
    exit 1
fi

echo "✅ uthreads.o created successfully"

# Create static library (optional)
echo "Step 2: Creating static library..."
ar rcs libuthreads.a uthreads.o

if [ $? -ne 0 ]; then
    echo "❌ ERROR: Failed to create static library"
    exit 1
fi

echo "✅ libuthreads.a created successfully"

# Compile test file if it exists
if [ -f "test_basic.c" ]; then
    echo "Step 3: Compiling test..."
    gcc $CFLAGS test_basic.c uthreads.o -o test_basic
    
    if [ $? -ne 0 ]; then
        echo "❌ ERROR: Failed to compile test"
        exit 1
    fi
    
    echo "✅ test_basic compiled successfully"
    echo "Run with: ./test_basic"
else
    echo "ℹ️  No test_basic.c found - skipping test compilation"
fi

echo "🎉 All compilation completed successfully!"
echo ""
echo "Files created:"
echo "  - uthreads.o      (object file)"
echo "  - libuthreads.a   (static library)"
if [ -f "test_basic" ]; then
    echo "  - test_basic      (test executable)"
fi