#!/bin/bash

# FastResize - Build Pre-compiled Binaries
# This script builds binaries for multiple platforms

set -e

echo "🔨 FastResize Binary Builder"
echo "============================="
echo ""

# Get version from CMakeLists.txt
VERSION=$(grep "project(fast_resize VERSION" CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
echo "Version: $VERSION"

# Detect platform
OS="unknown"
ARCH=$(uname -m)
if [[ "$OSTYPE" == "darwin"* ]]; then
    OS="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
    OS="linux"
fi

echo "Platform: $OS-$ARCH"
echo ""

# Output directory
OUTPUT_DIR="bindings/ruby/prebuilt/$OS-$ARCH"
mkdir -p "$OUTPUT_DIR/lib"
mkdir -p "$OUTPUT_DIR/bin"

echo "🔧 Building for $OS-$ARCH..."

# Clean and build
rm -rf build
mkdir build
cd build

echo "🔧 Building static library..."

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

    # Debug: Check the library
    echo "🔍 Checking library file:"
    file build/libfastresize.a
    ls -la build/libfastresize.a

    # Get library size
    if [[ "$ARCH" == "aarch64" ]]; then
        echo "Library size: $(stat -c%s build/libfastresize.a 2>/dev/null || stat -f%z build/libfastresize.a) bytes"
    else
        echo "Library size: $(stat -c%s build/libfastresize.a) bytes"
    fi

    # Copy the static library
    cp build/libfastresize.a "$OUTPUT_DIR/lib/libfastresize.a"

    # Copy CLI binary
    if [ -f build/fast_resize ]; then
        cp build/fast_resize "$OUTPUT_DIR/bin/fast_resize"
        echo "✅ Built CLI binary for Linux $ARCH"
    fi

    echo "✅ Built static library for Linux $ARCH"
else
    # macOS: Copy static library and CLI binary
    cp build/libfastresize.a "$OUTPUT_DIR/lib/libfastresize.a"

    # Copy CLI binary
    if [ -f build/fast_resize ]; then
        cp build/fast_resize "$OUTPUT_DIR/bin/fast_resize"
        echo "✅ Built CLI binary for macOS"
    fi

    echo "✅ Built static library for macOS"
fi

# Copy headers
cp -r include "$OUTPUT_DIR/"

# Create tarball in prebuilt/ folder (not bindings/ruby/prebuilt/)
mkdir -p prebuilt
cd bindings/ruby/prebuilt
tar czf "../../../prebuilt/fast_resize-$VERSION-$OS-$ARCH.tar.gz" "$OS-$ARCH"
cd ../../..

# Create CLI-only tarball for standalone installation
if [ -f "$OUTPUT_DIR/bin/fast_resize" ]; then
    echo ""
    echo "📦 Creating standalone CLI tarball..."
    mkdir -p prebuilt/cli-temp
    cp "$OUTPUT_DIR/bin/fast_resize" prebuilt/cli-temp/
    cd prebuilt/cli-temp
    tar czf "../fast_resize-$VERSION-$OS-$ARCH-cli.tar.gz" fast_resize
    cd ../..
    rm -rf prebuilt/cli-temp
    echo "✅ CLI tarball: prebuilt/fast_resize-$VERSION-$OS-$ARCH-cli.tar.gz"
fi

echo ""
echo "✅ Binary built successfully!"
echo "📦 Output: prebuilt/fast_resize-$VERSION-$OS-$ARCH.tar.gz"
echo "📦 Ruby prebuilt: bindings/ruby/prebuilt/$OS-$ARCH/"
echo ""
echo "Contents:"
ls -lh "$OUTPUT_DIR/lib/" 2>/dev/null || true
ls -lh "$OUTPUT_DIR/bin/" 2>/dev/null || true
echo ""
echo "Prebuilt artifacts:"
ls -lh prebuilt/ 2>/dev/null || true
