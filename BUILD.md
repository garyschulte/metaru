# Building Besu Native EVM

This document explains how to build the native EVM library for use with Besu via Panama FFM.

## Architecture

This is a **Panama FFM** implementation with a single-file EVM:
- **Source**: `src/evm_optimized.cpp` (optimized EVM with direct stack writes)
- **Headers**: `include/message_frame_memory.h`, `include/storage_memory.h`, `include/account_witness.h`, `include/tracer_callback.h`
- **API**: `extern "C"` function `execute_message()` for Java Foreign Function & Memory API

## Quick Start

### Using the Build Script (Recommended)

```bash
./build.sh                    # Release build (auto-detect CMake)
./build.sh Debug              # Debug build
./build.sh Release direct     # Force direct compilation (no CMake)
./build.sh Release cmake      # Force CMake build
```

### Direct Compilation

```bash
# macOS
c++ -std=c++17 -shared -fPIC -O3 -march=native -o libbesu_native_evm.dylib \
    src/evm_optimized.cpp -I./include

# Linux
c++ -std=c++17 -shared -fPIC -O3 -march=native -o libbesu_native_evm.so \
    src/evm_optimized.cpp -I./include

# Windows (MinGW)
c++ -std=c++17 -shared -fPIC -O3 -march=native -o besu_native_evm.dll \
    src/evm_optimized.cpp -I./include
```

### Using CMake

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

CMake will automatically:
- Detect your platform (macOS/Linux/Windows)
- Apply optimal compiler flags
- Copy the library to `../besu/evm/src/test/resources/` (if Besu found)

## Build Options

### Build Types

- **Release** (default): `-O3 -march=native -ffast-math`
- **Debug**: `-g -O0` (for debugging with gdb/lldb)

### Compiler Flags

**Release optimizations:**
- `-O3`: Maximum optimization
- `-march=native`: Use all CPU features (AVX2, etc.)
- `-ffast-math`: Fast floating point (not critical for EVM)
- `-Wall -Wextra`: Enable warnings

**Debug:**
- `-g`: Debug symbols
- `-O0`: No optimization (easier debugging)

## Output

The build produces a shared library:
- **macOS**: `libbesu_native_evm.dylib` (~37KB direct, ~54KB CMake)
- **Linux**: `libbesu_native_evm.so`
- **Windows**: `besu_native_evm.dll`

### Exported Symbol

Only one symbol is exported:
```c
extern "C" void execute_message(MessageFrameMemory* frame, TracerCallbacks* tracer);
```

## Verification

### Check Build

```bash
# macOS
nm libbesu_native_evm.dylib | grep execute_message
otool -L libbesu_native_evm.dylib

# Linux
nm -D libbesu_native_evm.so | grep execute_message
ldd libbesu_native_evm.so
```

### Run Tests

```bash
cd ../besu
./gradlew :evm:test --tests NativeMessageProcessorTest
```

Expected output:
```
NativeMessageProcessorTest > testSimpleExecution() PASSED
NativeMessageProcessorTest > testReusableMemoryPerformance() PASSED
```

## Troubleshooting

### Library Not Found

If Java can't find the library:

1. **Copy to Besu resources:**
   ```bash
   cp libbesu_native_evm.dylib ../besu/evm/src/test/resources/
   ```

2. **Or set library path:**
   ```bash
   # macOS
   export DYLD_LIBRARY_PATH=/path/to/metaru:$DYLD_LIBRARY_PATH

   # Linux
   export LD_LIBRARY_PATH=/path/to/metaru:$LD_LIBRARY_PATH
   ```

3. **Or use Java property:**
   ```bash
   ./gradlew :evm:test -Djava.library.path=/path/to/metaru
   ```

### Compilation Errors

**Missing headers:**
- Ensure you're in the `metaru` directory
- Check `include/` directory exists

**Linker errors:**
- macOS: May need `-lc++`
- Linux: May need `-lpthread` for threading

**Symbol not found:**
- Check `extern "C"` is present in source
- Verify symbol with `nm` command

### CMake Issues

**CMake not found:**
- macOS: `brew install cmake`
- Ubuntu/Debian: `sudo apt-get install cmake`
- Or use direct compilation mode

**Besu path not detected:**
```bash
cmake -DBESU_PATH=/path/to/besu ..
```

## Cross-Platform Compatibility

### macOS (Darwin)
- Clang/LLVM compiler
- Uses `.dylib` extension
- ARM64 (M1/M2) and x86_64 supported

### Linux
- GCC or Clang
- Uses `.so` extension
- x86_64 and ARM64 supported

### Windows
- MinGW-w64 or MSVC
- Uses `.dll` extension
- Requires `-fPIC` equivalent on MSVC

## Performance Notes

### Optimization Levels

The default `-O3 -march=native` provides ~5% better performance than `-O2`:
- **-O3**: Aggressive inlining, vectorization
- **-march=native**: AVX2, SSE4.2 instructions (x86_64)

### Profile-Guided Optimization (Advanced)

For maximum performance:

```bash
# 1. Build with profiling
c++ -std=c++17 -shared -fPIC -O3 -march=native -fprofile-generate \
    -o libbesu_native_evm.dylib src/evm_optimized.cpp -I./include

# 2. Run benchmarks to generate profile data
cd ../besu
./gradlew :evm:jmh -Pjmh.includes=NativeMessageProcessor

# 3. Rebuild with profile data
cd ../metaru
c++ -std=c++17 -shared -fPIC -O3 -march=native -fprofile-use \
    -o libbesu_native_evm.dylib src/evm_optimized.cpp -I./include
```

This can provide 5-10% additional speedup.

## Build Artifacts

After building, you'll have:
- `libbesu_native_evm.dylib` (or .so/.dll)
- `build/` directory (if using CMake)
- `build-debug/` directory (for debug builds)

## Clean

```bash
rm -rf build build-debug libbesu_native_evm.*
```

## Documentation

- **STORAGE_DESIGN.md**: Multi-account storage architecture
- **WITNESS_ARCHITECTURE.md**: Transaction witness design
- **WITNESS_UPDATES.md**: Dynamic account creation
- **performance_summary.md**: Performance benchmarks

## Summary

✅ Single-file EVM (`evm_optimized.cpp`)
✅ Panama FFM compatible
✅ Cross-platform (macOS/Linux/Windows)
✅ Multiple build methods (script/direct/CMake)
✅ Optimized for performance (~5.5 ns/op without tracing)
✅ Auto-copy to Besu resources
