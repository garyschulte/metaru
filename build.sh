#!/bin/bash
# Build script for Besu Native EVM (Panama FFM architecture)

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Besu Native EVM Build Script ===${NC}"
echo -e "${BLUE}Architecture: Panama FFM (single-file EVM)${NC}"
echo ""

# Parse arguments
BUILD_TYPE="${1:-Release}"
USE_CMAKE="${2:-auto}"

if [ "$BUILD_TYPE" = "debug" ] || [ "$BUILD_TYPE" = "Debug" ]; then
    BUILD_TYPE="Debug"
fi

echo -e "${GREEN}Build type: $BUILD_TYPE${NC}"

# Check for CMake
if command -v cmake >/dev/null 2>&1; then
    HAS_CMAKE=true
    echo -e "${GREEN}CMake found: $(cmake --version | head -1)${NC}"
else
    HAS_CMAKE=false
    echo -e "${YELLOW}CMake not found${NC}"
fi

# Decide build method
if [ "$USE_CMAKE" = "direct" ]; then
    echo -e "${BLUE}Using direct compilation (forced)${NC}"
    HAS_CMAKE=false
elif [ "$USE_CMAKE" = "cmake" ]; then
    echo -e "${BLUE}Using CMake (forced)${NC}"
    if [ "$HAS_CMAKE" = false ]; then
        echo -e "${RED}Error: CMake not found but --cmake flag used${NC}"
        exit 1
    fi
elif [ "$HAS_CMAKE" = true ]; then
    echo -e "${BLUE}Using CMake (auto-detected)${NC}"
else
    echo -e "${BLUE}Using direct compilation (CMake not available)${NC}"
fi

echo ""

# Build with CMake
if [ "$HAS_CMAKE" = true ] && [ "$USE_CMAKE" != "direct" ]; then
    BUILD_DIR="build"
    if [ "$BUILD_TYPE" = "Debug" ]; then
        BUILD_DIR="build-debug"
    fi

    echo -e "${YELLOW}Build directory: $BUILD_DIR${NC}"
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    # Detect number of cores
    if [[ "$OSTYPE" == "darwin"* ]]; then
        CORES=$(sysctl -n hw.ncpu)
    else
        CORES=$(nproc)
    fi
    echo -e "${GREEN}Using $CORES CPU cores${NC}"

    # Configure
    echo -e "${YELLOW}Configuring with CMake...${NC}"
    cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

    # Build
    echo -e "${YELLOW}Building...${NC}"
    cmake --build . -j"$CORES"

    echo -e "${GREEN}Build complete!${NC}"

    if [[ "$OSTYPE" == "darwin"* ]]; then
        LIB_PATH="$(pwd)/libbesu_native_evm.dylib"
    else
        LIB_PATH="$(pwd)/libbesu_native_evm.so"
    fi

    echo -e "${GREEN}Library: $LIB_PATH${NC}"

    # Copy to parent directory for convenience
    cp "$LIB_PATH" ..
    echo -e "${GREEN}Copied to: $(dirname $(pwd))/libbesu_native_evm.*${NC}"

# Build directly with compiler
else
    echo -e "${YELLOW}Compiling directly with c++...${NC}"

    # Compiler flags
    if [ "$BUILD_TYPE" = "Debug" ]; then
        FLAGS="-std=c++17 -shared -fPIC -g -O0 -Wall -Wextra"
    else
        FLAGS="-std=c++17 -shared -fPIC -O3 -march=native -ffast-math -Wall -Wextra"
    fi

    # Output name
    if [[ "$OSTYPE" == "darwin"* ]]; then
        OUTPUT="libbesu_native_evm.dylib"
    elif [[ "$OSTYPE" == "linux"* ]]; then
        OUTPUT="libbesu_native_evm.so"
    elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "cygwin" ]]; then
        OUTPUT="besu_native_evm.dll"
    else
        OUTPUT="libbesu_native_evm.so"
    fi

    echo -e "${BLUE}Flags: $FLAGS${NC}"
    echo -e "${BLUE}Output: $OUTPUT${NC}"
    echo ""

    # Compile
    c++ $FLAGS -o "$OUTPUT" src/evm_optimized.cpp -I./include

    echo -e "${GREEN}Build complete!${NC}"
    echo -e "${GREEN}Library: $(pwd)/$OUTPUT${NC}"
fi

echo ""
echo -e "${GREEN}=== Next Steps ===${NC}"
echo ""
echo -e "${BLUE}1. Run tests:${NC}"
echo "   cd ../besu"
echo "   ./gradlew :evm:test --tests NativeMessageProcessorTest"
echo ""
echo -e "${BLUE}2. Copy to Besu resources (if not auto-copied):${NC}"
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "   cp libbesu_native_evm.dylib ../besu/evm/src/test/resources/"
else
    echo "   cp libbesu_native_evm.so ../besu/evm/src/test/resources/"
fi
echo ""
echo -e "${BLUE}3. View documentation:${NC}"
echo "   cat STORAGE_DESIGN.md"
echo "   cat WITNESS_ARCHITECTURE.md"
echo "   cat WITNESS_UPDATES.md"
echo ""

# Show library info
echo -e "${GREEN}=== Library Information ===${NC}"
if [[ "$OSTYPE" == "darwin"* ]]; then
    LIB_FILE="libbesu_native_evm.dylib"
    if [ -f "$LIB_FILE" ]; then
        echo -e "${BLUE}File size:${NC} $(ls -lh $LIB_FILE | awk '{print $5}')"
        echo -e "${BLUE}Symbols:${NC} $(nm $LIB_FILE | grep -c ' T '|| echo 0) functions exported"
        echo -e "${BLUE}Dependencies:${NC}"
        otool -L "$LIB_FILE" | tail -n +2
    fi
else
    LIB_FILE="libbesu_native_evm.so"
    if [ -f "$LIB_FILE" ]; then
        echo -e "${BLUE}File size:${NC} $(ls -lh $LIB_FILE | awk '{print $5}')"
        echo -e "${BLUE}Symbols:${NC} $(nm -D $LIB_FILE 2>/dev/null | grep -c ' T ' || echo 0) functions exported"
        echo -e "${BLUE}Dependencies:${NC}"
        ldd "$LIB_FILE" 2>/dev/null || echo "  (ldd not available)"
    fi
fi

echo ""
echo -e "${GREEN}Build complete! 🚀${NC}"
