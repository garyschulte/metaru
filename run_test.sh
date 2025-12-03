#!/bin/bash
# Script to build and test the Native EVM with Panama FFM

set -e

echo "=== Building Mock Native EVM Library ==="
echo ""

# Build the native library (with tracer callback support)
c++ -std=c++17 -shared -fPIC -o libbesu_native_evm.dylib src/mock_evm_with_tracer.cpp -I./include

if [ -f "libbesu_native_evm.dylib" ]; then
    echo "✓ Built: libbesu_native_evm.dylib"
    ls -lh libbesu_native_evm.dylib
    # Verify exported symbols
    echo ""
    echo "Exported symbols:"
    nm -gU libbesu_native_evm.dylib | grep execute
elif [ -f "libbesu_native_evm.so" ]; then
    echo "✓ Built: libbesu_native_evm.so"
    ls -lh libbesu_native_evm.so
    echo ""
    echo "Exported symbols:"
    nm -gD libbesu_native_evm.so | grep execute
fi

echo ""
echo "=== Running Besu Unit Tests ==="
echo ""

# Run tests (library path configured in besu/evm/build.gradle)
cd /Users/garyschulte/dev/besu
./gradlew :evm:test --tests NativeMessageProcessorTest --rerun-tasks

echo ""
echo "=== Test Complete ==="
echo ""
echo "Check detailed results at:"
echo "  file:///Users/garyschulte/dev/besu/evm/build/reports/tests/test/index.html"
