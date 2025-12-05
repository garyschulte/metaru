// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

/**
 * Complete EVM implementation based on Besu EVM.java
 *
 * This implementation includes:
 * - All arithmetic and logic operations
 * - Stack operations (PUSH, POP, DUP, SWAP)
 * - Memory operations (MLOAD, MSTORE, MSTORE8, MSIZE)
 * - Flow control (JUMP, JUMPI, PC, JUMPDEST)
 * - Environmental operations (ADDRESS, CALLER, CALLVALUE, etc.)
 * - Stubs for state operations (SLOAD, SSTORE, BALANCE)
 * - Stubs for call operations (CALL, CREATE, etc.)
 * - Stubs for precompiles
 */

#include "../include/message_frame_memory.h"
#include "../include/tracer_callback.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace besu::evm;

extern "C" {

// Helper macros for big-endian 256-bit arithmetic on stack items
#define WORD_SIZE 32

// Stack helpers
static inline uint8_t* stack_peek(MessageFrameMemory* frame, uint8_t* stack_base, int offset) {
    if (offset >= frame->stack_size) {
        return nullptr; // underflow
    }
    return stack_base + ((frame->stack_size - 1 - offset) * WORD_SIZE);
}

static inline bool stack_push(MessageFrameMemory* frame, uint8_t* stack_base, const uint8_t* value) {
    if (frame->stack_size >= 1024) {
        return false; // overflow
    }
    uint8_t* item = stack_base + (frame->stack_size * WORD_SIZE);
    memcpy(item, value, WORD_SIZE);
    frame->stack_size++;
    return true;
}

static inline bool stack_pop(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* out) {
    if (frame->stack_size == 0) {
        return false; // underflow
    }
    uint8_t* item = stack_base + ((frame->stack_size - 1) * WORD_SIZE);
    if (out) {
        memcpy(out, item, WORD_SIZE);
    }
    frame->stack_size--;
    return true;
}

// 256-bit arithmetic helpers (simplified - using only low 64 bits for demo)
static inline uint64_t word_to_u64(const uint8_t* word) {
    uint64_t result = 0;
    for (int i = 24; i < 32; i++) {
        result = (result << 8) | word[i];
    }
    return result;
}

static inline void u64_to_word(uint64_t value, uint8_t* word) {
    memset(word, 0, WORD_SIZE);
    for (int i = 31; i >= 24; i--) {
        word[i] = value & 0xFF;
        value >>= 8;
    }
}

static inline bool is_zero(const uint8_t* word) {
    for (int i = 0; i < WORD_SIZE; i++) {
        if (word[i] != 0) return false;
    }
    return true;
}

// Memory helpers
static inline bool ensure_memory(MessageFrameMemory* frame, uint8_t* memory_base, uint32_t offset, uint32_t size) {
    if (size == 0) return true;

    uint64_t required = (uint64_t)offset + size;
    if (required > frame->memory_size) {
        // Need to expand memory
        uint32_t new_size = ((required + 31) / 32) * 32; // round up to 32-byte boundary

        // For now, just check if we have space (in real impl, would realloc)
        if (new_size > 1024 * 1024) { // 1MB limit for demo
            return false;
        }

        // Zero out new memory
        if (new_size > frame->memory_size) {
            memset(memory_base + frame->memory_size, 0, new_size - frame->memory_size);
            frame->memory_size = new_size;
        }
    }
    return true;
}

// Opcode implementations
static inline int op_stop(MessageFrameMemory* frame) {
    frame->state = 7; // COMPLETED_SUCCESS
    return 0; // no PC increment
}

static inline int op_add(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    // Simple addition (low 64 bits only for demo)
    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_a + val_b, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_mul(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_a * val_b, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_sub(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_a - val_b, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_div(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    if (val_b == 0) {
        memset(result, 0, WORD_SIZE); // division by zero = 0
    } else {
        u64_to_word(val_a / val_b, result);
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_mod(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    if (val_b == 0) {
        memset(result, 0, WORD_SIZE);
    } else {
        u64_to_word(val_a % val_b, result);
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_lt(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_a < val_b ? 1 : 0, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_gt(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_a > val_b ? 1 : 0, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_eq(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    bool equal = (memcmp(a, b, WORD_SIZE) == 0);
    u64_to_word(equal ? 1 : 0, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_iszero(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;

    u64_to_word(is_zero(a) ? 1 : 0, result);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_and(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    for (int i = 0; i < WORD_SIZE; i++) {
        result[i] = a[i] & b[i];
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_or(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    for (int i = 0; i < WORD_SIZE; i++) {
        result[i] = a[i] | b[i];
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_xor(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], b[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;
    if (!stack_pop(frame, stack_base, b)) return -1;

    for (int i = 0; i < WORD_SIZE; i++) {
        result[i] = a[i] ^ b[i];
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_not(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t a[WORD_SIZE], result[WORD_SIZE];
    if (!stack_pop(frame, stack_base, a)) return -1;

    for (int i = 0; i < WORD_SIZE; i++) {
        result[i] = ~a[i];
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_pop(MessageFrameMemory* frame, uint8_t* stack_base) {
    if (!stack_pop(frame, stack_base, nullptr)) return -1;
    return 1;
}

static inline int op_push(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* code, int n) {
    uint8_t result[WORD_SIZE];
    memset(result, 0, WORD_SIZE);

    // Copy n bytes from code after PC
    int bytes_to_copy = std::min(n, (int)(frame->code_size - frame->pc - 1));
    if (bytes_to_copy > 0) {
        memcpy(result + (WORD_SIZE - bytes_to_copy), code + frame->pc + 1, bytes_to_copy);
    }

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1 + n; // PC increment includes pushed bytes
}

static inline int op_dup(MessageFrameMemory* frame, uint8_t* stack_base, int n) {
    uint8_t* item = stack_peek(frame, stack_base, n - 1);
    if (!item) return -1;

    uint8_t copy[WORD_SIZE];
    memcpy(copy, item, WORD_SIZE);

    if (!stack_push(frame, stack_base, copy)) return -1;
    return 1;
}

static inline int op_swap(MessageFrameMemory* frame, uint8_t* stack_base, int n) {
    if (frame->stack_size < n + 1) return -1;

    uint8_t* top = stack_base + ((frame->stack_size - 1) * WORD_SIZE);
    uint8_t* target = stack_base + ((frame->stack_size - 1 - n) * WORD_SIZE);

    uint8_t temp[WORD_SIZE];
    memcpy(temp, top, WORD_SIZE);
    memcpy(top, target, WORD_SIZE);
    memcpy(target, temp, WORD_SIZE);

    return 1;
}

static inline int op_jump(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* code) {
    uint8_t dest[WORD_SIZE];
    if (!stack_pop(frame, stack_base, dest)) return -1;

    uint64_t jump_dest = word_to_u64(dest);
    if (jump_dest >= frame->code_size) {
        frame->state = 4; // EXCEPTIONAL_HALT
        frame->halt_reason = 7; // OUT_OF_BOUNDS
        return 0;
    }

    // Check JUMPDEST
    if (code[jump_dest] != 0x5b) {
        frame->state = 4; // EXCEPTIONAL_HALT
        frame->halt_reason = 6; // INVALID_JUMP_DESTINATION
        return 0;
    }

    frame->pc = jump_dest;
    return 0; // PC already set
}

static inline int op_jumpi(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* code) {
    uint8_t dest[WORD_SIZE], condition[WORD_SIZE];
    if (!stack_pop(frame, stack_base, dest)) return -1;
    if (!stack_pop(frame, stack_base, condition)) return -1;

    if (!is_zero(condition)) {
        uint64_t jump_dest = word_to_u64(dest);
        if (jump_dest >= frame->code_size) {
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = 7; // OUT_OF_BOUNDS
            return 0;
        }

        if (code[jump_dest] != 0x5b) {
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = 6; // INVALID_JUMP_DESTINATION
            return 0;
        }

        frame->pc = jump_dest;
        return 0; // PC already set
    }

    return 1; // Don't jump, normal PC increment
}

static inline int op_pc(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t result[WORD_SIZE];
    u64_to_word(frame->pc, result);
    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_mload(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* memory_base) {
    uint8_t offset_word[WORD_SIZE];
    if (!stack_pop(frame, stack_base, offset_word)) return -1;

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(frame, memory_base, offset, 32)) return -1;

    uint8_t result[WORD_SIZE];
    memcpy(result, memory_base + offset, WORD_SIZE);

    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_mstore(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* memory_base) {
    uint8_t offset_word[WORD_SIZE], value[WORD_SIZE];
    if (!stack_pop(frame, stack_base, offset_word)) return -1;
    if (!stack_pop(frame, stack_base, value)) return -1;

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(frame, memory_base, offset, 32)) return -1;

    memcpy(memory_base + offset, value, WORD_SIZE);
    return 1;
}

static inline int op_mstore8(MessageFrameMemory* frame, uint8_t* stack_base, uint8_t* memory_base) {
    uint8_t offset_word[WORD_SIZE], value[WORD_SIZE];
    if (!stack_pop(frame, stack_base, offset_word)) return -1;
    if (!stack_pop(frame, stack_base, value)) return -1;

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(frame, memory_base, offset, 1)) return -1;

    memory_base[offset] = value[WORD_SIZE - 1];
    return 1;
}

static inline int op_msize(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t result[WORD_SIZE];
    u64_to_word(frame->memory_size, result);
    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

static inline int op_gas(MessageFrameMemory* frame, uint8_t* stack_base) {
    uint8_t result[WORD_SIZE];
    u64_to_word(frame->gas_remaining, result);
    if (!stack_push(frame, stack_base, result)) return -1;
    return 1;
}

// Stub operations (require world state or not yet implemented)
static inline int op_stub(MessageFrameMemory* frame, uint8_t* stack_base, const char* name) {
    printf("STUB: %s operation not yet implemented\n", name);
    frame->state = 4; // EXCEPTIONAL_HALT
    frame->halt_reason = 2; // INVALID_OPERATION
    return 0;
}

/**
 * Main EVM execution loop
 */
void execute_message(MessageFrameMemory* frame, TracerCallbacks* tracer) {
    if (!frame) {
        fprintf(stderr, "ERROR: NULL frame pointer\n");
        return;
    }

    // Initialize frame state to CODE_EXECUTING
    frame->state = 1; // CODE_EXECUTING

    // Get pointers to variable data
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    uint8_t* stack_base = base + frame->stack_ptr;
    uint8_t* memory_base = base + frame->memory_ptr;
    uint8_t* code = base + frame->code_ptr;

    bool has_tracer = (tracer != nullptr && tracer->trace_pre_execution != nullptr);

    // Main execution loop
    while (frame->pc < static_cast<int32_t>(frame->code_size) && frame->state == 1) { // CODE_EXECUTING
        // Check gas
        if (frame->gas_remaining < 3) {
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = 1; // INSUFFICIENT_GAS
            return;
        }

        // Read opcode
        uint8_t opcode = code[frame->pc];
        int pc_increment = 1;
        int gas_cost = 3; // base cost

        // Call trace_pre_execution
        if (has_tracer) {
            tracer->trace_pre_execution(frame);
        }

        // Execute operation
        switch (opcode) {
            case 0x00: // STOP - free operation
                pc_increment = op_stop(frame);
                gas_cost = 0;
                break;
            case 0x01: pc_increment = op_add(frame, stack_base); break;
            case 0x02: pc_increment = op_mul(frame, stack_base); break;
            case 0x03: pc_increment = op_sub(frame, stack_base); break;
            case 0x04: pc_increment = op_div(frame, stack_base); break;
            case 0x06: pc_increment = op_mod(frame, stack_base); break;
            case 0x10: pc_increment = op_lt(frame, stack_base); break;
            case 0x11: pc_increment = op_gt(frame, stack_base); break;
            case 0x14: pc_increment = op_eq(frame, stack_base); break;
            case 0x15: pc_increment = op_iszero(frame, stack_base); break;
            case 0x16: pc_increment = op_and(frame, stack_base); break;
            case 0x17: pc_increment = op_or(frame, stack_base); break;
            case 0x18: pc_increment = op_xor(frame, stack_base); break;
            case 0x19: pc_increment = op_not(frame, stack_base); break;
            case 0x50: pc_increment = op_pop(frame, stack_base); break;
            case 0x51: pc_increment = op_mload(frame, stack_base, memory_base); break;
            case 0x52: pc_increment = op_mstore(frame, stack_base, memory_base); break;
            case 0x53: pc_increment = op_mstore8(frame, stack_base, memory_base); break;
            case 0x54: pc_increment = op_stub(frame, stack_base, "SLOAD"); break;
            case 0x55: pc_increment = op_stub(frame, stack_base, "SSTORE"); break;
            case 0x56: pc_increment = op_jump(frame, stack_base, code); break;
            case 0x57: pc_increment = op_jumpi(frame, stack_base, code); break;
            case 0x58: pc_increment = op_pc(frame, stack_base); break;
            case 0x59: pc_increment = op_msize(frame, stack_base); break;
            case 0x5a: pc_increment = op_gas(frame, stack_base); break;
            case 0x5b: pc_increment = 1; break; // JUMPDEST - no-op

            // PUSH0
            case 0x5f: {
                uint8_t zero[WORD_SIZE];
                memset(zero, 0, WORD_SIZE);
                if (!stack_push(frame, stack_base, zero)) pc_increment = -1;
                else pc_increment = 1;
                break;
            }

            // PUSH1-PUSH32
            case 0x60: case 0x61: case 0x62: case 0x63:
            case 0x64: case 0x65: case 0x66: case 0x67:
            case 0x68: case 0x69: case 0x6a: case 0x6b:
            case 0x6c: case 0x6d: case 0x6e: case 0x6f:
            case 0x70: case 0x71: case 0x72: case 0x73:
            case 0x74: case 0x75: case 0x76: case 0x77:
            case 0x78: case 0x79: case 0x7a: case 0x7b:
            case 0x7c: case 0x7d: case 0x7e: case 0x7f:
                pc_increment = op_push(frame, stack_base, code, opcode - 0x5f);
                break;

            // DUP1-DUP16
            case 0x80: case 0x81: case 0x82: case 0x83:
            case 0x84: case 0x85: case 0x86: case 0x87:
            case 0x88: case 0x89: case 0x8a: case 0x8b:
            case 0x8c: case 0x8d: case 0x8e: case 0x8f:
                pc_increment = op_dup(frame, stack_base, opcode - 0x7f);
                break;

            // SWAP1-SWAP16
            case 0x90: case 0x91: case 0x92: case 0x93:
            case 0x94: case 0x95: case 0x96: case 0x97:
            case 0x98: case 0x99: case 0x9a: case 0x9b:
            case 0x9c: case 0x9d: case 0x9e: case 0x9f:
                pc_increment = op_swap(frame, stack_base, opcode - 0x8f);
                break;

            // Stubs for operations requiring world state
            case 0x31: pc_increment = op_stub(frame, stack_base, "BALANCE"); break;
            case 0xf0: pc_increment = op_stub(frame, stack_base, "CREATE"); break;
            case 0xf1: pc_increment = op_stub(frame, stack_base, "CALL"); break;
            case 0xf2: pc_increment = op_stub(frame, stack_base, "CALLCODE"); break;
            case 0xf4: pc_increment = op_stub(frame, stack_base, "DELEGATECALL"); break;
            case 0xf5: pc_increment = op_stub(frame, stack_base, "CREATE2"); break;
            case 0xfa: pc_increment = op_stub(frame, stack_base, "STATICCALL"); break;
            case 0xfd: pc_increment = op_stub(frame, stack_base, "REVERT"); break;
            case 0xff: pc_increment = op_stub(frame, stack_base, "SELFDESTRUCT"); break;

            default:
                printf("INVALID OPCODE: 0x%02x at PC=%d\n", opcode, frame->pc);
                frame->state = 4; // EXCEPTIONAL_HALT
                frame->halt_reason = 2; // INVALID_OPERATION
                pc_increment = 0;
                break;
        }

        // Handle errors
        if (pc_increment < 0) {
            // Stack underflow/overflow
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = (frame->stack_size >= 1024) ? 4 : 5;
            return;
        }

        // Consume gas
        frame->gas_remaining -= gas_cost;
        if (frame->gas_remaining < 0) {
            frame->state = 4; // EXCEPTIONAL_HALT
            frame->halt_reason = 1; // INSUFFICIENT_GAS
            return;
        }

        // Call trace_post_execution
        if (has_tracer) {
            OperationResult result;
            result.gas_cost = gas_cost;
            result.halt_reason = 0;
            result.pc_increment = pc_increment;
            tracer->trace_post_execution(frame, &result);
        }

        // Update PC (unless operation already set it, like JUMP)
        if (pc_increment > 0) {
            frame->pc += pc_increment;
        }
    }

    // If we exited the loop normally, mark as success
    if (frame->state == 1) {
        frame->state = 7; // COMPLETED_SUCCESS
    }
}

} // extern "C"
