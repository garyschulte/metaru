// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

/**
 * Mock native EVM implementation for testing Panama FFM interop.
 *
 * This is a minimal implementation that:
 * - Reads the MessageFrameMemory from shared memory
 * - Performs simple operations (increment PC, push to stack)
 * - Writes results back to shared memory
 * - Sets completion state
 */

#include "../include/message_frame_memory.h"
#include <cstdio>
#include <cstring>

using namespace besu::evm;

extern "C" {

/**
 * Execute a message frame (mock implementation).
 *
 * This mock implementation:
 * 1. Reads PC and gas from shared memory
 * 2. Increments PC by 1
 * 3. Consumes 3 gas
 * 4. If stack has items, pops 2 and pushes their sum
 * 5. Sets state to COMPLETED_SUCCESS
 *
 * @param frame_memory Pointer to shared memory containing MessageFrameMemory
 * @param tracer_callback Pointer to OperationTracer callback (unused in mock)
 */
void execute_message(MessageFrameMemory* frame, void* tracer_callback) {
    if (!frame) {
        fprintf(stderr, "ERROR: NULL frame pointer\n");
        return;
    }

    // Log entry
    printf("Native EVM: execute_message called\n");
    printf("  Initial PC: %d\n", frame->pc);
    printf("  Initial gas: %lld\n", (long long)frame->gas_remaining);
    printf("  Initial stack size: %d\n", frame->stack_size);
    printf("  Initial memory size: %d\n", frame->memory_size);

    // Simple operation: increment PC
    frame->pc += 1;

    // Consume 3 gas
    if (frame->gas_remaining >= 3) {
        frame->gas_remaining -= 3;
    } else {
        // Out of gas
        frame->state = 4; // EXCEPTIONAL_HALT
        frame->halt_reason = 1; // INSUFFICIENT_GAS
        printf("  OUT OF GAS\n");
        return;
    }

    // Simple stack operation: if we have 2+ items, pop 2 and push their sum
    if (frame->stack_size >= 2) {
        // Get base pointer for stack
        uint8_t* base = reinterpret_cast<uint8_t*>(frame);
        uint8_t* stack_base = base + frame->stack_ptr;

        // Read top two items (items are 32 bytes each)
        uint8_t* item0 = stack_base + ((frame->stack_size - 1) * 32);
        uint8_t* item1 = stack_base + ((frame->stack_size - 2) * 32);

        // Simple addition (treat as big-endian, only handle last byte for simplicity)
        uint8_t sum = item0[31] + item1[31];

        // Pop 2 items
        frame->stack_size -= 2;

        // Push result (zero-fill except last byte)
        uint8_t* result = stack_base + (frame->stack_size * 32);
        memset(result, 0, 32);
        result[31] = sum;

        // Increment stack size
        frame->stack_size += 1;

        printf("  Stack operation: popped 2, pushed 1 (sum=%d)\n", sum);
    }

    // Write some output data (for testing)
    if (frame->output_ptr != 0) {
        uint8_t* output = reinterpret_cast<uint8_t*>(frame) + frame->output_ptr;
        const char* msg = "NATIVE_EVM_SUCCESS";
        int msg_len = strlen(msg);
        memcpy(output, msg, msg_len);
        frame->output_size = msg_len;
        printf("  Set output: %s\n", msg);
    }

    // Set state to COMPLETED_SUCCESS
    frame->state = 7; // COMPLETED_SUCCESS

    printf("  Final PC: %d\n", frame->pc);
    printf("  Final gas: %lld\n", (long long)frame->gas_remaining);
    printf("  Final stack size: %d\n", frame->stack_size);
    printf("  Final state: %d\n", frame->state);
}

} // extern "C"
