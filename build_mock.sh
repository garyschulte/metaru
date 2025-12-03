#!/bin/bash
# Build script for mock native EVM library

set -e  # Exit on error

echo "Building mock native EVM library..."

# Create build directory
mkdir -p build_mock
cd build_mock

# Run CMake with mock configuration
cmake -DCMAKE_BUILD_TYPE=Release -f ../CMakeLists.mock.txt ..

# Build
cmake --build . --config Release

echo ""
echo "Build complete!"
echo ""

# Show output
if [ -f "libbesu_native_evm.dylib" ]; then
    echo "Created: $(pwd)/libbesu_native_evm.dylib"
    ls -lh libbesu_native_evm.dylib
elif [ -f "libbesu_native_evm.so" ]; then
    echo "Created: $(pwd)/libbesu_native_evm.so"
    ls -lh libbesu_native_evm.so
elif [ -f "besu_native_evm.dll" ]; then
    echo "Created: $(pwd)/besu_native_evm.dll"
    ls -lh besu_native_evm.dll
else
    echo "Warning: Library not found in expected location"
    find . -name "*besu_native_evm*"
fi

echo ""
echo "To use this library, set:"
if [ "$(uname)" == "Darwin" ]; then
    echo "  export DYLD_LIBRARY_PATH=$(pwd):\$DYLD_LIBRARY_PATH"
else
    echo "  export LD_LIBRARY_PATH=$(pwd):\$LD_LIBRARY_PATH"
fi
