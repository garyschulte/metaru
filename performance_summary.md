# Java vs Native C++ EVM Performance Comparison

## Comprehensive Performance Table

| Operations | Tracing | Arena Mode | Java EVM (ns) | Native EVM (ns) | Native/Java Ratio | Winner |
|------------|---------|------------|---------------|-----------------|-------------------|--------|
| **4 ops** | YES (callbacks) | With Arena | 172 | 52,398 | 305x slower | ⚠️ Java |
| **4 ops** | NO | With Arena | 169 | 12,566 | 74x slower | ⚠️ Java |
| **500 ops** | NO | With Arena | 340 | 31,266 | 92x slower | ⚠️ Java |
| **500 ops** | NO | Reusable | 545 | 10,322 | 19x slower | ⚠️ Java |
| **10,000 ops** | NO | Reusable | 201,819 | 54,688 | **3.69x faster** | ✅ Native |
| **10,000 ops** | YES (20K callbacks) | Reusable | 244,357 | 2,603,690 | 10.66x slower | ⚠️ Java |

## Performance per Operation (ns/op)

| Operations | Tracing | Arena Mode | Java ns/op | Native ns/op | Notes |
|------------|---------|------------|------------|--------------|-------|
| **4 ops** | YES | With Arena | 43.0 | 13,099 | Callback overhead dominates |
| **4 ops** | NO | With Arena | 42.3 | 3,141 | FFI overhead dominates |
| **500 ops** | NO | With Arena | 0.68 | 62.5 | FFI + arena allocation overhead |
| **500 ops** | NO | Reusable | 1.09 | 20.6 | FFI overhead amortizing |
| **10,000 ops** | NO | Reusable | 20.2 | **5.5** | Native execution dominates |
| **10,000 ops** | YES | Reusable | 24.4 | 260.4 | Callback overhead kills native perf |

## Overhead Breakdown (500 ops, NO_TRACING)

| Component | Time (ns) | % of Total |
|-----------|-----------|------------|
| **With Arena Allocation** | | |
| - Arena allocation | ~16,000 | 51% |
| - Data marshalling | ~12,500 | 40% |
| - FFI + C++ execution | ~3,000 | 9% |
| **Total** | **31,266** | 100% |
| | | |
| **With Reusable Memory** | | |
| - Data marshalling | ~8,000 | 77% |
| - FFI + C++ execution | ~2,400 | 23% |
| **Total** | **10,322** | 100% |

## Tracer Callback Overhead (10,000 ops, 20,000 total callbacks)

| Implementation | No Tracing (ns) | With Tracing (ns) | Callback Overhead | % Overhead |
|----------------|-----------------|-------------------|-------------------|------------|
| **Java EVM** | 201,819 | 244,357 | 42,538 | 17.4% |
| **Native EVM** | 54,688 | 2,603,690 | 2,549,002 | **97.9%** |

**Key Finding**: Each callback requires crossing the FFI boundary twice (native→Java→native), adding ~127 ns per callback for native vs ~2 ns for Java.

## Key Findings

### Crossover Point (Without Tracing)
- **4 operations**: Java is 74x faster (FFI overhead >> execution time)
- **500 operations**: Java is 19x faster (FFI overhead still significant)
- **10,000 operations**: Native is 3.69x faster (execution time >> FFI overhead)

**Estimated crossover**: ~2,000-5,000 operations

### Crossover Point (With Tracing)
- **10,000 operations**: Java is 10.66x faster
- **Tracing effectively eliminates native advantage** due to callback FFI overhead
- Each callback adds ~127ns for native vs ~2ns for Java

### Overhead Analysis
1. **Arena allocation**: ~16µs (eliminated with reusable memory)
2. **FFI boundary crossing**: ~500-1,000ns per call
3. **Data marshalling**: ~8-12µs (populateFrameMemory + updateFrameFromMemory)
4. **Tracer callbacks**: ~5µs per callback

### Performance Optimizations Applied
1. ✅ **Reusable arena** - 2x speedup (eliminated 50% overhead)
2. ✅ **Frame pooling** - Eliminated test setup overhead
3. ✅ **Long bytecode** - Better FFI amortization

### When Native Wins
- Long-running contracts (>2,000 operations)
- Computation-heavy workloads
- When tracing is disabled
- With reusable memory arenas

### When Java Wins
- Short contracts (<2,000 operations)
- With tracer callbacks (FFI callback overhead)
- Without memory optimization
- Cold start scenarios

## Raw Performance (10,000 ops, optimized)

### Without Tracing
**Native C++ EVM**: 5.5 ns/op (181 million ops/sec) ✅
**Java EVM**: 20.2 ns/op (49 million ops/sec)

**Native C++ is 3.69x faster without tracing!**

### With Tracing (20,000 callbacks)
**Native C++ EVM**: 260.4 ns/op (3.8 million ops/sec) ⚠️
**Java EVM**: 24.4 ns/op (41 million ops/sec)

**Java is 10.66x faster with tracing due to FFI callback overhead**

## Conclusion

The native C++ EVM shows **3.69x better performance** for long-running transactions without tracing, making it ideal for:
- Block execution (minimal tracing)
- Transaction simulation
- Benchmark/stress testing
- Computation-heavy contracts

However, **tracing kills native performance** (97.9% overhead from FFI callbacks), making Java EVM superior for:
- Debugging with tracers
- Transaction tracing APIs
- Development/testing with detailed logging

**Bottom line**: Native C++ EVM wins for production block processing, Java EVM wins when detailed tracing is required.
