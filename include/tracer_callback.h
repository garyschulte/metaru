// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace besu {
namespace evm {

// Forward declaration
struct MessageFrameMemory;

/**
 * Operation result passed to tracePostExecution.
 * Simplified version for mock implementation.
 */
struct OperationResult {
    int64_t gas_cost;           // Gas cost of the operation
    uint32_t halt_reason;       // ExceptionalHaltReason (0 = none)
    uint32_t pc_increment;      // How much to increment PC (usually 1)
};

/**
 * Tracer callback function pointers.
 *
 * These are upcalls from C++ -> Java using Panama FFM.
 * The Java side will create MemorySegments pointing to these functions.
 */
struct TracerCallbacks {
    /**
     * Called before executing each operation.
     *
     * @param frame Pointer to MessageFrameMemory in shared memory
     */
    void (*trace_pre_execution)(MessageFrameMemory* frame);

    /**
     * Called after executing each operation.
     *
     * @param frame Pointer to MessageFrameMemory in shared memory
     * @param result Operation result (gas cost, halt reason, pc increment)
     */
    void (*trace_post_execution)(MessageFrameMemory* frame, OperationResult* result);
};

} // namespace evm
} // namespace besu
