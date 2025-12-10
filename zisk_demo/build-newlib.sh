#!/bin/bash
set -e  # Exit on error

# Configuration
NEWLIB_VERSION="4.4.0.20231231"
NEWLIB_URL="ftp://sourceware.org/pub/newlib/newlib-${NEWLIB_VERSION}.tar.gz"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/newlib-build"
INSTALL_DIR="${SCRIPT_DIR}/newlib-rv64im"
SOURCE_DIR="${BUILD_DIR}/newlib-${NEWLIB_VERSION}"

# Target configuration
TARGET="riscv64-unknown-elf"
ARCH="rv64im"
ABI="lp64"
CMODEL="medany"

echo "========================================="
echo "Building newlib for ${TARGET}"
echo "Architecture: ${ARCH}"
echo "ABI: ${ABI}"
echo "Code model: ${CMODEL}"
echo "Install directory: ${INSTALL_DIR}"
echo "========================================="

# Check for required tools
if ! command -v ${TARGET}-gcc &> /dev/null; then
    echo "Error: ${TARGET}-gcc not found in PATH"
    echo "Please install the RISC-V GNU toolchain first"
    exit 1
fi

# Clean up previous build if it exists
if [ -d "${BUILD_DIR}" ]; then
    echo "Cleaning up previous build directory..."
    rm -rf "${BUILD_DIR}"
fi

# Create build directory
echo "Creating build directory..."
mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

# Download newlib if not already present
if [ ! -f "newlib-${NEWLIB_VERSION}.tar.gz" ]; then
    echo "Downloading newlib ${NEWLIB_VERSION}..."
    curl -O "${NEWLIB_URL}" || wget "${NEWLIB_URL}" || {
        echo "Error: Failed to download newlib"
        echo "Please download manually from: ${NEWLIB_URL}"
        exit 1
    }
else
    echo "Using cached newlib-${NEWLIB_VERSION}.tar.gz"
fi

# Extract
echo "Extracting newlib..."
tar xzf "newlib-${NEWLIB_VERSION}.tar.gz"

# Create build subdirectory (out-of-tree build)
mkdir -p build-newlib
cd build-newlib

# Configure newlib
echo "Configuring newlib..."
CFLAGS_FOR_TARGET="-march=${ARCH} -mabi=${ABI} -mcmodel=${CMODEL} -Os -g" \
"${SOURCE_DIR}/configure" \
    --target=${TARGET} \
    --prefix="${INSTALL_DIR}" \
    --enable-newlib-nano-malloc \
    --enable-newlib-nano-formatted-io \
    --enable-newlib-reent-small \
    --disable-newlib-fvwrite-in-streamio \
    --disable-newlib-fseek-optimization \
    --disable-newlib-wide-orient \
    --disable-newlib-unbuf-stream-opt \
    --enable-newlib-global-atexit \
    --enable-lite-exit \
    --disable-multilib \
    --enable-newlib-global-stdio-streams

# Build
echo "Building newlib (this may take a few minutes)..."
CPU_COUNT=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
make -j${CPU_COUNT}

# Install
echo "Installing newlib to ${INSTALL_DIR}..."
make install

# Clean up build directory
cd "${SCRIPT_DIR}"
echo "Cleaning up build directory..."
rm -rf "${BUILD_DIR}"

echo ""
echo "========================================="
echo "newlib build complete!"
echo "Installed to: ${INSTALL_DIR}"
echo ""
echo "You can now build your project with:"
echo "  make"
echo "========================================="
