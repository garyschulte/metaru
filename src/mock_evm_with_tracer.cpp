// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

/**
 * Mock EVM implementation with tracer callbacks.
 *
 * This mock simulates executing actual EVM operations:
 * - PUSH1 5   (0x60 0x05)
 * - PUSH1 3   (0x60 0x03)
 * - ADD       (0x01)
 * - STOP      (0x00)
 *
 * It calls tracePreExecution before each operation and tracePostExecution after,
 * allowing us to measure native->Java callback performance.
 */

#include "../include/message_frame_memory.h"
#include "../include/tracer_callback.h"
#include <cstdio>
#include <cstring>

using namespace besu::evm;

extern "C" {

/**
 * Execute message with tracer callbacks.
 *
 * This simulates a simple EVM execution loop:
 * 1. Read opcode from code at PC
 * 2. Call trace_pre_execution
 * 3. Execute operation (update stack, gas, etc.)
 * 4. Call trace_post_execution
 * 5. Update PC
 * 6. Repeat until STOP or out of gas
 *
 * @param frame Pointer to shared MessageFrameMemory
 * @param tracer Pointer to TracerCallbacks struct (can be NULL for NO_TRACING)
 */
void execute_message(MessageFrameMemory* frame, TracerCallbacks* tracer) {
    if (!frame) {
        fprintf(stderr, "ERROR: NULL frame pointer\n");
        return;
    }

    // printf("=== Mock EVM Execution Started ===\n");
    // printf("Initial PC: %d\n", frame->pc);
    // printf("Initial gas: %lld\n", (long long)frame->gas_remaining);
    // printf("Initial stack size: %d\n", frame->stack_size);
    // printf("Code size: %d bytes\n", frame->code_size);

    // Get pointers to variable data
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    uint8_t* stack_base = base + frame->stack_ptr;
    uint8_t* code = base + frame->code_ptr;

    // Debug: Print pointers
    // printf("Pointer offsets:\n");
    // printf("  stack_ptr: %llu\n", (unsigned long long)frame->stack_ptr);
    // printf("  memory_ptr: %llu\n", (unsigned long long)frame->memory_ptr);
    // printf("  code_ptr: %llu\n", (unsigned long long)frame->code_ptr);
    // printf("  Stack will use: %d bytes (from %llu to %llu)\n",
    //        frame->stack_size * 32, (unsigned long long)frame->stack_ptr,
    //        (unsigned long long)(frame->stack_ptr + frame->stack_size * 32));

    // Debug: Print first 10 bytes of code
    // printf("Code bytes:");
    // for (uint32_t i = 0; i < frame->code_size && i < 10; i++) {
    //     printf(" %02x", code[i]);
    // }
    // printf("\n");

    int operation_count = 0;
    bool has_tracer = (tracer != nullptr && tracer->trace_pre_execution != nullptr);

    // Execution loop
    while (frame->pc < static_cast<int32_t>(frame->code_size)) {
        // Check gas
        if (frame->gas_remaining < 3) {
            // printf("OUT OF GAS at PC=%d\n", frame->pc);
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = 1; // INSUFFICIENT_GAS
            return;
        }

        // Read opcode
        uint8_t opcode = code[frame->pc];
        operation_count++;

        // printf("\n--- Operation %d ---\n", operation_count);
        // printf("PC: %d, code[PC]: 0x%02x, code[PC-1]: 0x%02x, code[PC+1]: 0x%02x\n",
        //        frame->pc,
        //        code[frame->pc],
        //        (frame->pc > 0) ? code[frame->pc - 1] : 0,
        //        (frame->pc < static_cast<int32_t>(frame->code_size - 1)) ? code[frame->pc + 1] : 0);
        // printf("Opcode: 0x%02x, Gas: %lld, Stack size: %d\n",
        //        opcode, (long long)frame->gas_remaining, frame->stack_size);

        // Prepare operation result
        OperationResult result;
        result.gas_cost = 3;
        result.halt_reason = 0;
        result.pc_increment = 1;

        // Call trace_pre_execution
        if (has_tracer) {
            tracer->trace_pre_execution(frame);
        }

        // Execute operation
        switch (opcode) {
            case 0x00: { // STOP
                // printf("  -> STOP\n");
                result.gas_cost = 0;
                frame->state = 7; // COMPLETED_SUCCESS

                // Call trace_post_execution
                if (has_tracer) {
                    tracer->trace_post_execution(frame, &result);
                }

                // printf("\n=== Execution completed successfully ===\n");
                // printf("Final PC: %d\n", frame->pc);
                // printf("Final gas: %lld\n", (long long)frame->gas_remaining);
                // printf("Final stack size: %d\n", frame->stack_size);
                // printf("Total operations: %d\n", operation_count);
                return;
            }

            case 0x01: { // ADD
                if (frame->stack_size < 2) {
                    // printf("  -> ADD: STACK UNDERFLOW\n");
                    frame->state = 4; // EXCEPTIONAL_HALT
                    frame->halt_reason = 5; // STACK_UNDERFLOW
                    return;
                }

                // Pop two items
                uint8_t* item1 = stack_base + ((frame->stack_size - 1) * 32);
                uint8_t* item0 = stack_base + ((frame->stack_size - 2) * 32);

                // Simple addition (last byte only for demo)
                uint8_t sum = item0[31] + item1[31];
                // printf("  -> ADD: %d + %d = %d\n", item0[31], item1[31], sum);

                // Pop 2
                frame->stack_size -= 2;

                // Push result
                uint8_t* result_item = stack_base + (frame->stack_size * 32);
                memset(result_item, 0, 32);
                result_item[31] = sum;
                frame->stack_size += 1;

                break;
            }

            case 0x60: { // PUSH1
                if (frame->pc + 1 >= static_cast<int32_t>(frame->code_size)) {
                    // printf("  -> PUSH1: OUT OF BOUNDS\n");
                    frame->state = 4; // EXCEPTIONAL_HALT
                    frame->halt_reason = 7; // OUT_OF_BOUNDS
                    return;
                }

                if (frame->stack_size >= 1024) {
                    // printf("  -> PUSH1: STACK OVERFLOW\n");
                    frame->state = 4; // EXCEPTIONAL_HALT
                    frame->halt_reason = 4; // STACK_OVERFLOW
                    return;
                }

                uint8_t value = code[frame->pc + 1];
                // printf("  -> PUSH1: 0x%02x (%d)\n", value, value);

                // Push value
                uint8_t* item = stack_base + (frame->stack_size * 32);
                memset(item, 0, 32);
                item[31] = value;
                frame->stack_size += 1;

                result.pc_increment = 2; // PUSH1 consumes 2 bytes
                break;
            }

            default:
                // printf("  -> INVALID OPCODE: 0x%02x\n", opcode);
                frame->state = 4; // EXCEPTIONAL_HALT
                frame->halt_reason = 2; // INVALID_OPERATION
                return;
        }

        // Consume gas
        frame->gas_remaining -= result.gas_cost;

        // Call trace_post_execution
        if (has_tracer) {
            tracer->trace_post_execution(frame, &result);
        }

        // Update PC
        frame->pc += result.pc_increment;
    }

    // printf("\n=== Execution completed (end of code) ===\n");
    frame->state = 7; // COMPLETED_SUCCESS
}

} // extern "C"
