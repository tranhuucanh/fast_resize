#!/bin/bash

# FastResize - Build Pre-compiled Binaries
# This script builds binaries for multiple platforms with full static linking

set -e

echo "🔨 FastResize Binary Builder"
echo "============================="
echo ""

# Get version from VERSION file
VERSION=$(cat VERSION)
echo "Version: $VERSION"

# Detect platform
OS="unknown"
ARCH=$(uname -m)
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
fi

# Normalize architecture name
if [[ "$ARCH" == "aarch64" ]]; then
    ARCH_NORMALIZED="aarch64"
elif [[ "$ARCH" == "arm64" ]]; then
    ARCH_NORMALIZED="arm64"
elif [[ "$ARCH" == "x86_64" ]]; then
    ARCH_NORMALIZED="x86_64"
else
    ARCH_NORMALIZED="$ARCH"
fi

echo "Platform: $OS-$ARCH_NORMALIZED"
echo ""

# Output directory - consistent with gem structure
OUTPUT_DIR="bindings/ruby/prebuilt/$OS-$ARCH_NORMALIZED"
mkdir -p "$OUTPUT_DIR/lib"
mkdir -p "$OUTPUT_DIR/bin"

echo "🔧 Building for $OS-$ARCH_NORMALIZED..."

# Clean and build
rm -rf build
mkdir build
cd build

echo "🔧 Building static library and CLI..."

# Set PKG_CONFIG_PATH to find custom-built libraries
if [[ "$OS" == "linux" ]]; then
    export PKG_CONFIG_PATH="/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH"
    echo "PKG_CONFIG_PATH: $PKG_CONFIG_PATH"
fi

# Configure for static library and CLI build
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DFASTRESIZE_STATIC=ON \
    -DFASTRESIZE_BUILD_CLI=ON \
    -DFASTRESIZE_BUILD_TESTS=OFF \
    -DFASTRESIZE_BUILD_EXAMPLES=OFF \
    -DFASTRESIZE_BUILD_BENCHMARKS=OFF \
    -DCMAKE_INSTALL_PREFIX="$PWD/install"

# Build
if [[ "$OS" == "macos" ]]; then
    make -j$(sysctl -n hw.ncpu)
else
    make -j$(nproc)
fi

cd ..

# Copy library and CLI binary to prebuilt structure
if [[ "$OS" == "linux" ]]; then
    echo "🔧 Packaging static library and CLI for Linux..."

    # Debug: Check the binary
    echo "🔍 Checking binary file:"
    file build/fast_resize
    ls -la build/fast_resize

    # Get binary size
    echo "Binary size: $(stat -c%s build/fast_resize 2>/dev/null || stat -f%z build/fast_resize) bytes"

    # Check binary dependencies
    echo "📋 Binary dependencies:"
    ldd build/fast_resize 2>&1 || echo "✅ Binary is statically linked (ldd failed - this is GOOD!)"

    # Test if binary runs
    echo "🧪 Testing binary execution:"
    ./build/fast_resize --version || echo "⚠️ Binary test failed"

    # Copy the static library
    cp build/libfastresize.a "$OUTPUT_DIR/lib/libfastresize.a"

    # Copy CLI binary
    if [ -f build/fast_resize ]; then
        cp build/fast_resize "$OUTPUT_DIR/bin/fast_resize"
        chmod +x "$OUTPUT_DIR/bin/fast_resize"
        echo "✅ Built CLI binary for Linux $ARCH_NORMALIZED"
    fi

    echo "✅ Built static library for Linux $ARCH_NORMALIZED"
else
    # macOS: Copy static library and CLI binary
    cp build/libfastresize.a "$OUTPUT_DIR/lib/libfastresize.a"

    # Copy CLI binary
    if [ -f build/fast_resize ]; then
        cp build/fast_resize "$OUTPUT_DIR/bin/fast_resize"
        chmod +x "$OUTPUT_DIR/bin/fast_resize"
        echo "✅ Built CLI binary for macOS $ARCH_NORMALIZED"
    fi

    echo "✅ Built static library for macOS $ARCH_NORMALIZED"
fi

# Copy headers
cp -r include "$OUTPUT_DIR/"

# Create tarball in prebuilt/ folder (for GitHub release)
mkdir -p prebuilt
cd bindings/ruby/prebuilt
tar czf "../../../prebuilt/fast_resize-$VERSION-$OS-$ARCH_NORMALIZED.tar.gz" "$OS-$ARCH_NORMALIZED"
cd ../../..

echo ""
echo "✅ Binary built successfully!"
echo "📦 Output: prebuilt/fast_resize-$VERSION-$OS-$ARCH_NORMALIZED.tar.gz"
echo "📦 Ruby prebuilt: bindings/ruby/prebuilt/$OS-$ARCH_NORMALIZED/"
echo ""
echo "Contents:"
ls -lh "$OUTPUT_DIR/lib/" 2>/dev/null || true
ls -lh "$OUTPUT_DIR/bin/" 2>/dev/null || true
echo ""
echo "Prebuilt artifacts:"
ls -lh prebuilt/ 2>/dev/null || true
