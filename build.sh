#!/bin/bash
# Build script for Besu Native EVM

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Besu Native EVM Build Script ===${NC}"

# Check for required tools
command -v cmake >/dev/null 2>&1 || { echo -e "${RED}Error: cmake is required but not installed.${NC}" >&2; exit 1; }
command -v java >/dev/null 2>&1 || { echo -e "${RED}Error: java is required but not installed.${NC}" >&2; exit 1; }

# Detect number of cores
if [[ "$OSTYPE" == "darwin"* ]]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=$(nproc)
fi

echo -e "${GREEN}Detected $CORES CPU cores${NC}"

# Parse arguments
BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

if [ "$BUILD_TYPE" = "debug" ] || [ "$BUILD_TYPE" = "Debug" ]; then
    BUILD_TYPE="Debug"
    BUILD_DIR="build-debug"
fi

echo -e "${GREEN}Build type: $BUILD_TYPE${NC}"
echo -e "${GREEN}Build directory: $BUILD_DIR${NC}"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure
echo -e "${YELLOW}Configuring with CMake...${NC}"
cmake -DCMAKE_BUILD_TYPE="$BUILD_TYPE" ..

# Build
echo -e "${YELLOW}Building...${NC}"
cmake --build . -j"$CORES"

# Run tests if available
if [ -f "besu_native_evm_test" ]; then
    echo -e "${YELLOW}Running tests...${NC}"
    ctest --output-on-failure
else
    echo -e "${YELLOW}Tests not built (Google Test not found)${NC}"
fi

echo -e "${GREEN}Build complete!${NC}"
echo -e "${GREEN}Library: ${BUILD_DIR}/libbesu_native_evm.*${NC}"

# Show next steps
echo ""
echo -e "${GREEN}=== Next Steps ===${NC}"
echo "1. Copy library to Java library path:"
echo "   cp ${BUILD_DIR}/libbesu_native_evm.* \$JAVA_HOME/lib/"
echo ""
echo "2. Or set library path when running Besu:"
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "   export DYLD_LIBRARY_PATH=$(pwd)/${BUILD_DIR}:\$DYLD_LIBRARY_PATH"
else
    echo "   export LD_LIBRARY_PATH=$(pwd)/${BUILD_DIR}:\$LD_LIBRARY_PATH"
fi
echo ""
echo "3. Or use Java system property:"
echo "   java -Djava.library.path=$(pwd)/${BUILD_DIR} ..."
