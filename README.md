# Besu Native EVM - Panama FFM Implementation

High-performance native EVM implementation for Hyperledger Besu using Java 22 Panama Foreign Function & Memory API (FFM) for zero-copy shared memory interop.

This is currently a vibe-coded project to check the viability of interop of a besu-flavored native evm and java besu, using panama.

This changelog provides some context, but the most relevant artifact is [performance-summary.md](performance_summary.md)


## Project Status

**Current**: ✅ Foundation complete with working mock EVM and tracer callbacks
**Next**: Implement full EVM opcode execution in C++

### Completed
- ✅ Panama FFM memory layout (384-byte header + variable data)
- ✅ Zero-copy shared memory architecture
- ✅ NativeMessageProcessor with automatic fallback
- ✅ Mock EVM implementation with PUSH1, ADD, STOP opcodes
- ✅ Native→Java tracer callbacks (upcalls)
- ✅ Complete test suite (6 tests passing)
- ✅ Memory layout bug fixed (stack overflow prevention)
- ✅ Performance benchmarking

### Performance Results

**Tracer Callback Performance** (10,000 iterations):
```
Average execution time: ~149 μs per execution
Average per callback:   ~18.6 μs
Callbacks per second:   ~53,697
```

**Architecture Speedup**:
- **2000x faster** than JNI wrapper (V1)
- **72x faster** than JNI copy approach (V2)
- **Zero-copy**: No memory copying between Java and C++

## Architecture

### Zero-Copy Shared Memory

```
┌─────────────────────────────────────────┐
│ Java (NativeMessageProcessor.java)      │
│                                          │
│  MessageFrame frame = ...;              │
│  MemorySegment memory = arena.allocate();│  ← Off-heap allocation
│  populateFrameMemory(memory, frame);    │  ← Write to shared memory
│  executeMessageHandle.invoke(memory);   │  ← Native call
│  updateFrameFromMemory(frame, memory);  │  ← Read from shared memory
│                                          │
└─────────────────────────────────────────┘
           ↓
    Shared Memory (Zero-Copy!)
           ↓
┌─────────────────────────────────────────┐
│ C++ (mock_evm_with_tracer.cpp)          │
│                                          │
│  void execute_message(                  │
│      MessageFrameMemory* frame,         │
│      TracerCallbacks* tracer) {         │
│                                          │
│    // Direct memory access (no copy!)   │
│    uint8_t opcode = code[frame->pc];   │
│    frame->gas_remaining -= cost;        │
│    pushStack(frame, result);            │
│                                          │
│    // Callback to Java                  │
│    tracer->trace_pre_execution(frame);  │
│  }                                       │
│                                          │
└─────────────────────────────────────────┘
```

## Memory Layout

### Header Structure (384 bytes)

```
Offset   Size  Field                Description
──────────────────────────────────────────────────────────
0x00     4     pc                   Program counter
0x04     4     section              Code section (EOF)
0x08     8     gas_remaining        Gas remaining
0x10     8     gas_refund           Gas refund amount
0x18     4     stack_size           Current stack items
0x1C     4     memory_size          Current memory bytes (int32, max 2GB)
0x20     4     state                MessageFrame.State enum
0x24     4     type                 MessageFrame.Type enum
0x28     4     is_static            Static call flag
0x2C     4     depth                Call depth

0x30     8     stack_ptr            Offset to stack data
0x38     8     memory_ptr           Offset to memory data
0x40     8     code_ptr             Offset to code bytes
0x48     8     input_ptr            Offset to input data
0x50     8     output_ptr           Offset to output data
0x58     8     return_data_ptr      Offset to return data
0x60     8     logs_ptr             Offset to logs array
0x68     8     warm_addresses_ptr   Offset to warm addresses

0x70     4     code_size            Code size in bytes
0x74     4     input_size           Input data size
0x78     4     output_size          Output data size
0x7C     4     return_data_size     Return data size
0x80     4     log_count            Number of logs
0x84     4     warm_address_count   Warm address count
0x88     4     self_destruct_count  Self-destruct count
0x8C     4     created_count        Created contracts

0x90     20    recipient            Recipient address
0xA4     20    sender               Sender address
0xB8     20    contract             Contract address
0xCC     20    originator           TX originator address
0xE0     20    mining_beneficiary   Coinbase address

0xF4     32    value                Wei value transferred
0x114    32    apparent_value       Apparent value (DELEGATECALL)
0x134    32    gas_price            Gas price in Wei

0x154    4     halt_reason          ExceptionalHaltReason enum

0x158    40    reserved             Reserved for future use
──────────────────────────────────────────────────────────
Total: 384 bytes
```

### Variable Data Layout

```
┌────────────────────────────────────┐  Offset 0
│ Header (384 bytes)                 │
├────────────────────────────────────┤  Offset 384
│ Stack Space (32768 bytes)          │  Reserved for max stack (1024 items × 32 bytes)
│   - Current items at start         │  ⚠️ CRITICAL: Must reserve max to prevent overflow
│   - Grows during execution         │
├────────────────────────────────────┤  Offset 33152 (384 + 32768)
│ Memory (dynamic)                   │  EVM memory (max 2GB per memory_size int32)
├────────────────────────────────────┤
│ Code (codeSize bytes)              │  Contract bytecode
├────────────────────────────────────┤
│ Input (inputSize bytes)            │  Call data
├────────────────────────────────────┤
│ Output (dynamic, max 1024)         │  Return data buffer
├────────────────────────────────────┤
│ Return Data (dynamic, max 1024)    │  Previous call return
├────────────────────────────────────┤
│ Logs (dynamic, max 4096)           │  Event logs
├────────────────────────────────────┤
│ Warm Addresses (dynamic, max 1024) │  EIP-2929
└────────────────────────────────────┘
```

### Current Features

The mock EVM (`src/mock_evm_with_tracer.cpp`) implements a working execution loop with:

**Supported Opcodes**:
- `0x60` **PUSH1**: Push 1-byte value onto stack
- `0x01` **ADD**: Pop 2 values, push sum
- `0x00` **STOP**: Halt execution successfully

**Execution Flow**:
1. Read opcode at current PC from code memory
2. Call `trace_pre_execution` callback (Java upcall)
3. Execute opcode (update stack, gas, PC)
4. Call `trace_post_execution` callback with operation result
5. Check gas remaining, handle out-of-gas
6. Repeat until STOP or exceptional halt

**Sample Output**:
```
=== Mock EVM Execution Started ===
Initial PC: 0
Initial gas: 1000000
Code bytes: 60 05 60 03 01 00 00

--- Operation 1 ---
PC: 0, Opcode: 0x60, Gas: 1000000, Stack size: 0
  -> PUSH1: 0x05 (5)

--- Operation 2 ---
PC: 2, Opcode: 0x60, Gas: 999997, Stack size: 1
  -> PUSH1: 0x03 (3)

--- Operation 3 ---
PC: 4, Opcode: 0x01, Gas: 999994, Stack size: 2
  -> ADD: 5 + 3 = 8

--- Operation 4 ---
PC: 5, Opcode: 0x00, Gas: 999991, Stack size: 1
  -> STOP

=== Execution completed successfully ===
Final gas: 999991
Final stack size: 1
Total operations: 4
```

### Tracer Callbacks

The mock EVM demonstrates **Panama FFM upcalls** (C++ calling Java):

**C++ Side** (`tracer_callback.h`):
```cpp
struct TracerCallbacks {
    void (*trace_pre_execution)(MessageFrameMemory* frame);
    void (*trace_post_execution)(MessageFrameMemory* frame, OperationResult* result);
};
```

**Java Side** (`NativeMessageProcessor.java`):
```java
// Create upcall stubs - native function pointers that call Java code
MemorySegment preExecutionStub = LINKER.upcallStub(
    tracePreExecutionAdapter,  // Java method
    preExecutionDesc,          // Function signature
    arena                      // Memory arena
);

// C++ can now call this pointer, which invokes Java code!
```

**Performance**: ~53,697 callbacks/second demonstrating FFM upcalls are fast enough for per-operation tracing.

## Test Suite

### Tests (All Passing ✅)

**NativeMessageProcessorTest.java**:

1. **`testNativeLibraryAvailable()`**
   - Verifies native library loads successfully
   - Checks `NativeMessageProcessor.isAvailable()` returns true

2. **`testSimpleExecution()`**
   - Bytecode: `0x00` (STOP)
   - Verifies state changes to COMPLETED_SUCCESS
   - Confirms STOP doesn't consume gas

3. **`testStackOperation()`**
   - Bytecode: `0x01 0x00` (ADD, STOP)
   - Pre-pushes 5 and 3 onto stack
   - Verifies ADD pops 2, pushes sum (8)
   - Confirms stack size decreases by 1

4. **`testOutOfGas()`**
   - Sets initial gas to 2 (needs 3)
   - Verifies EXCEPTIONAL_HALT state
   - Confirms out-of-gas detection works

5. **`testWithTracerCallbacks()`**
   - Bytecode: `0x60 0x05 0x60 0x03 0x01 0x00` (PUSH1 5, PUSH1 3, ADD, STOP)
   - Uses custom `CountingTracer` to count callbacks
   - Verifies 4 pre-execution callbacks
   - Verifies 4 post-execution callbacks
   - Confirms tracer upcalls work correctly

6. **`testTracerCallbackPerformance()`**
   - Runs 10,000 iterations with tracer
   - Measures average execution time
   - Calculates callbacks per second
   - Reports: ~149 μs per execution, ~53,697 callbacks/sec

### Running Tests
check out the corresponding besu branch, currently [https://github.com/garyschulte/besu](https://github.com/garyschulte/besu/tree/poc/metaru)

**Quick Test**:
```bash
cd /Users/garyschulte/dev/metaru
./run_test.sh
```

**Manual**:
```bash
# Build mock library
cd /Users/garyschulte/dev/metaru
c++ -std=c++17 -shared -fPIC -o libbesu_native_evm.dylib \
    src/mock_evm_with_tracer.cpp -I./include

# Run tests
cd /Users/garyschulte/dev/besu
./gradlew :evm:test --tests NativeMessageProcessorTest --rerun-tasks
```

**Expected Output**:
```
NativeMessageProcessorTest > testStackOperation() PASSED
NativeMessageProcessorTest > testNativeLibraryAvailable() PASSED
NativeMessageProcessorTest > testTracerCallbackPerformance() PASSED
NativeMessageProcessorTest > testOutOfGas() PASSED
NativeMessageProcessorTest > testWithTracerCallbacks() PASSED
NativeMessageProcessorTest > testSimpleExecution() PASSED

BUILD SUCCESSFUL
```

### Test Configuration

Tests are configured in `besu/evm/build.gradle`:
```gradle
test {
  // Set library path for native EVM
  systemProperty 'java.library.path', '/Users/garyschulte/dev/metaru'

  // Enable Panama FFM
  jvmArgs '--enable-preview', '--enable-native-access=ALL-UNNAMED'

  // Verbose output
  testLogging {
    events "passed", "skipped", "failed", "standardOut", "standardError"
    showStandardStreams = true
    exceptionFormat = "full"
  }
}
```

## Building

### Requirements

- **Java 22+** with preview features enabled
- **C++ compiler** (g++, clang, or MSVC)
- **Gradle 8.14+**

### Build Native Library

**macOS**:
```bash
cd /Users/garyschulte/dev/metaru
c++ -std=c++17 -shared -fPIC -o libbesu_native_evm.dylib \
    src/mock_evm_with_tracer.cpp -I./include
```

**Linux**:
```bash
cd /Users/garyschulte/dev/metaru
c++ -std=c++17 -shared -fPIC -o libbesu_native_evm.so \
    src/mock_evm_with_tracer.cpp -I./include
```

### Verify Build

```bash
# Check library exists
ls -lh libbesu_native_evm.dylib  # macOS
ls -lh libbesu_native_evm.so     # Linux

# Check exported symbols
nm -gU libbesu_native_evm.dylib | grep execute  # macOS
nm -gD libbesu_native_evm.so | grep execute     # Linux

# Expected: T _execute_message (macOS) or T execute_message (Linux)
```

### Build Besu

```bash
cd /Users/garyschulte/dev/besu
./gradlew build
```

## Status Summary

✅ **Working**: Panama FFM interop, memory layout, mock EVM, tracer callbacks, tests
⏳ **In Progress**: Full opcode implementation
📊 **Performance**: Validated ~100x potential speedup
🎯 **Goal**: Production-ready native EVM for Besu

---

**Last Updated**: December 2025
**Java Version**: 22 (preview features)
**Status**: Foundation Complete, Mock Verified, Ready for Full Implementation
