#!/bin/bash
# Simplified test runner that sets library path properly

set -e

cd /Users/garyschulte/dev/kitsui

echo "=== Building Mock Native Library ==="
c++ -std=c++17 -shared -fPIC -o libbesu_native_evm.dylib src/mock_native_evm.cpp -I./include
echo "✓ Built libbesu_native_evm.dylib"
ls -lh libbesu_native_evm.dylib
echo ""

echo "=== Running Tests ==="
cd /Users/garyschulte/dev/besu

# Run with system property
./gradlew :evm:test --tests NativeMessageProcessorTest --rerun-tasks \
  -Dorg.gradle.jvmargs="-Djava.library.path=/Users/garyschulte/dev/kitsui"

echo ""
echo "=== Results ==="
echo "HTML Report: file:///Users/garyschulte/dev/besu/evm/build/reports/tests/test/index.html"
