// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

/**
 * Optimized EVM implementation with direct stack writes.
 * Eliminates intermediate copies for maximum performance.
 */

#include "../include/message_frame_memory.h"
#include "../include/storage_memory.h"
#include "../include/tracer_callback.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

using namespace besu::evm;

extern "C" {

#define WORD_SIZE 32

struct OpResult {
    int pc_increment;
    int gas_cost;
};

struct ExecutionContext {
    MessageFrameMemory* frame;
    uint8_t* stack_base;
    uint8_t* memory_base;
    const uint8_t* code;
    StorageEntry* storage_base;
};

// Fast stack helpers - return pointers for direct manipulation
static inline uint8_t* stack_top(ExecutionContext* ctx, int offset) {
    if (offset >= ctx->frame->stack_size) return nullptr;
    return ctx->stack_base + ((ctx->frame->stack_size - 1 - offset) * WORD_SIZE);
}

static inline uint8_t* stack_alloc(ExecutionContext* ctx) {
    if (ctx->frame->stack_size >= 1024) return nullptr;
    uint8_t* item = ctx->stack_base + (ctx->frame->stack_size * WORD_SIZE);
    ctx->frame->stack_size++;
    return item;
}

static inline bool stack_free(ExecutionContext* ctx, int count) {
    if (ctx->frame->stack_size < count) return false;
    ctx->frame->stack_size -= count;
    return true;
}

// 256-bit helpers
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
static inline bool ensure_memory(ExecutionContext* ctx, uint32_t offset, uint32_t size) {
    if (size == 0) return true;
    uint64_t required = (uint64_t)offset + size;
    if (required > ctx->frame->memory_size) {
        uint32_t new_size = ((required + 31) / 32) * 32;
        if (new_size > 1024 * 1024) return false;
        if (new_size > ctx->frame->memory_size) {
            memset(ctx->memory_base + ctx->frame->memory_size, 0, new_size - ctx->frame->memory_size);
            ctx->frame->memory_size = new_size;
        }
    }
    return true;
}

// ===== OPTIMIZED OPERATION HANDLERS (DIRECT STACK WRITES) =====

static OpResult op_stop(ExecutionContext* ctx) {
    ctx->frame->state = 7;
    return {0, 0};
}

static OpResult op_add(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);

    // Write result directly to top-1, then pop top
    u64_to_word(val_a + val_b, b);
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_mul(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    u64_to_word(word_to_u64(a) * word_to_u64(b), b);
    stack_free(ctx, 1);

    return {1, 5};
}

static OpResult op_sub(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    u64_to_word(word_to_u64(a) - word_to_u64(b), b);
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_div(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_b == 0 ? 0 : val_a / val_b, b);
    stack_free(ctx, 1);

    return {1, 5};
}

static OpResult op_mod(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    uint64_t val_a = word_to_u64(a);
    uint64_t val_b = word_to_u64(b);
    u64_to_word(val_b == 0 ? 0 : val_a % val_b, b);
    stack_free(ctx, 1);

    return {1, 5};
}

static OpResult op_lt(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    u64_to_word(word_to_u64(a) < word_to_u64(b) ? 1 : 0, b);
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_gt(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    u64_to_word(word_to_u64(a) > word_to_u64(b) ? 1 : 0, b);
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_eq(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    u64_to_word(memcmp(a, b, WORD_SIZE) == 0 ? 1 : 0, b);
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_iszero(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    if (!a) return {-1, 0};

    u64_to_word(is_zero(a) ? 1 : 0, a);

    return {1, 3};
}

static OpResult op_and(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    for (int i = 0; i < WORD_SIZE; i++) {
        b[i] &= a[i];
    }
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_or(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    for (int i = 0; i < WORD_SIZE; i++) {
        b[i] |= a[i];
    }
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_xor(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    uint8_t* b = stack_top(ctx, 1);
    if (!a || !b) return {-1, 0};

    for (int i = 0; i < WORD_SIZE; i++) {
        b[i] ^= a[i];
    }
    stack_free(ctx, 1);

    return {1, 3};
}

static OpResult op_not(ExecutionContext* ctx) {
    uint8_t* a = stack_top(ctx, 0);
    if (!a) return {-1, 0};

    for (int i = 0; i < WORD_SIZE; i++) {
        a[i] = ~a[i];
    }

    return {1, 3};
}

static OpResult op_pop(ExecutionContext* ctx) {
    if (!stack_free(ctx, 1)) return {-1, 0};
    return {1, 2};
}

static OpResult op_mload(ExecutionContext* ctx) {
    uint8_t* offset_word = stack_top(ctx, 0);
    if (!offset_word) return {-1, 0};

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(ctx, offset, 32)) return {-1, 0};

    // Write directly to stack top
    memcpy(offset_word, ctx->memory_base + offset, WORD_SIZE);

    return {1, 3};
}

static OpResult op_mstore(ExecutionContext* ctx) {
    uint8_t* offset_word = stack_top(ctx, 0);
    uint8_t* value = stack_top(ctx, 1);
    if (!offset_word || !value) return {-1, 0};

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(ctx, offset, 32)) return {-1, 0};

    memcpy(ctx->memory_base + offset, value, WORD_SIZE);
    stack_free(ctx, 2);

    return {1, 3};
}

static OpResult op_mstore8(ExecutionContext* ctx) {
    uint8_t* offset_word = stack_top(ctx, 0);
    uint8_t* value_word = stack_top(ctx, 1);
    if (!offset_word || !value_word) return {-1, 0};

    uint32_t offset = (uint32_t)word_to_u64(offset_word);
    if (!ensure_memory(ctx, offset, 1)) return {-1, 0};

    ctx->memory_base[offset] = value_word[31];
    stack_free(ctx, 2);

    return {1, 3};
}

static OpResult op_sload(ExecutionContext* ctx) {
    uint8_t* key_word = stack_top(ctx, 0);
    if (!key_word) return {-1, 0};

    // Get the current contract address (where storage is being read from)
    const uint8_t* address = ctx->frame->contract;

    // Look up storage entry by (address, key)
    StorageEntry* entry = storage::find(
        ctx->storage_base,
        ctx->frame->storage_slot_count,
        address,
        key_word
    );

    if (entry) {
        // Found the entry - copy value to stack
        memcpy(key_word, entry->value, WORD_SIZE);
        int gas_cost = entry->is_warm ? 100 : 2100;
        entry->is_warm = 1;  // Mark as warm
        return {1, gas_cost};
    }

    // Not found - return zero (empty storage slot)
    memset(key_word, 0, WORD_SIZE);
    return {1, 2100};  // Cold access
}

static OpResult op_sstore(ExecutionContext* ctx) {
    // Static calls cannot modify storage
    if (ctx->frame->is_static) {
        ctx->frame->state = 4;  // EXCEPTIONAL_HALT
        ctx->frame->halt_reason = 6;  // ILLEGAL_STATE_CHANGE
        return {-1, 0};
    }

    uint8_t* key_word = stack_top(ctx, 0);
    uint8_t* value_word = stack_top(ctx, 1);
    if (!key_word || !value_word) return {-1, 0};

    // Get the current contract address (where storage is being written)
    const uint8_t* address = ctx->frame->contract;

    // Look up storage entry by (address, key)
    StorageEntry* entry = storage::find(
        ctx->storage_base,
        ctx->frame->storage_slot_count,
        address,
        key_word
    );

    if (entry) {
        // Found existing entry - calculate gas cost (EIP-2200)
        bool is_zero_value = is_zero(value_word);
        bool was_zero_original = is_zero(entry->original);
        bool was_zero_current = is_zero(entry->value);

        int gas_cost;
        bool is_warm = entry->is_warm;

        if (is_zero_value) {
            // Setting to zero
            if (!was_zero_current) {
                // Clearing storage - refund
                ctx->frame->gas_refund += 4800;
            }
            gas_cost = is_warm ? 100 : 2100;
        } else {
            // Setting to non-zero
            if (was_zero_current && !was_zero_original) {
                // Re-setting (was cleared earlier)
                gas_cost = is_warm ? 100 : 2100;
            } else if (was_zero_current) {
                // First time setting
                gas_cost = 20000;
            } else {
                // Modifying existing
                gas_cost = is_warm ? 100 : 2100;
            }
        }

        // Update value and mark as warm
        memcpy(entry->value, value_word, WORD_SIZE);
        entry->is_warm = 1;

        stack_free(ctx, 2);
        return {1, gas_cost};
    }

    // Entry not found - add new entry
    entry = storage::add(
        ctx->storage_base,
        &ctx->frame->storage_slot_count,
        ctx->frame->max_storage_slots,
        address,
        key_word
    );

    if (!entry) {
        // Out of storage space
        ctx->frame->state = 4;
        ctx->frame->halt_reason = 2;  // INVALID_OPERATION
        return {-1, 0};
    }

    // Set value in new entry
    memcpy(entry->value, value_word, WORD_SIZE);
    memcpy(entry->original, value_word, WORD_SIZE);
    entry->is_warm = 1;

    stack_free(ctx, 2);

    // New storage slot (cold, non-zero value)
    return {1, 20000};
}

static OpResult op_jump(ExecutionContext* ctx) {
    uint8_t* dest_word = stack_top(ctx, 0);
    if (!dest_word) return {-1, 0};

    uint32_t dest = (uint32_t)word_to_u64(dest_word);
    if (dest >= ctx->frame->code_size || ctx->code[dest] != 0x5b) {
        ctx->frame->state = 4;
        ctx->frame->halt_reason = 3;
        return {-1, 0};
    }

    stack_free(ctx, 1);
    ctx->frame->pc = dest;
    return {0, 8};
}

static OpResult op_jumpi(ExecutionContext* ctx) {
    uint8_t* dest_word = stack_top(ctx, 0);
    uint8_t* cond_word = stack_top(ctx, 1);
    if (!dest_word || !cond_word) return {-1, 0};

    bool should_jump = !is_zero(cond_word);
    uint32_t dest = (uint32_t)word_to_u64(dest_word);

    stack_free(ctx, 2);

    if (should_jump) {
        if (dest >= ctx->frame->code_size || ctx->code[dest] != 0x5b) {
            ctx->frame->state = 4;
            ctx->frame->halt_reason = 3;
            return {-1, 0};
        }
        ctx->frame->pc = dest;
        return {0, 10};
    }

    return {1, 10};
}

static OpResult op_pc(ExecutionContext* ctx) {
    uint8_t* item = stack_alloc(ctx);
    if (!item) return {-1, 0};

    u64_to_word(ctx->frame->pc, item);
    return {1, 2};
}

static OpResult op_gas(ExecutionContext* ctx) {
    uint8_t* item = stack_alloc(ctx);
    if (!item) return {-1, 0};

    u64_to_word(ctx->frame->gas_remaining, item);
    return {1, 2};
}

static OpResult op_jumpdest(ExecutionContext* ctx) {
    return {1, 1};
}

static OpResult op_push0(ExecutionContext* ctx) {
    uint8_t* item = stack_alloc(ctx);
    if (!item) return {-1, 0};

    memset(item, 0, WORD_SIZE);
    return {1, 2};
}

static OpResult op_push_n(ExecutionContext* ctx, int n) {
    uint8_t* item = stack_alloc(ctx);
    if (!item) return {-1, 0};

    // Zero entire word first
    memset(item, 0, WORD_SIZE);

    // Copy bytes directly from code (like mock EVM)
    int bytes_to_copy = std::min(n, (int)(ctx->frame->code_size - ctx->frame->pc - 1));
    if (bytes_to_copy > 0) {
        memcpy(item + (WORD_SIZE - bytes_to_copy),
               ctx->code + ctx->frame->pc + 1,
               bytes_to_copy);
    }

    return {1 + n, 3};
}

static OpResult op_dup_n(ExecutionContext* ctx, int n) {
    uint8_t* source = stack_top(ctx, n - 1);
    if (!source) return {-1, 0};

    uint8_t* dest = stack_alloc(ctx);
    if (!dest) return {-1, 0};

    memcpy(dest, source, WORD_SIZE);
    return {1, 3};
}

static OpResult op_swap_n(ExecutionContext* ctx, int n) {
    uint8_t* top = stack_top(ctx, 0);
    uint8_t* other = stack_top(ctx, n);
    if (!top || !other) return {-1, 0};

    uint8_t temp[WORD_SIZE];
    memcpy(temp, top, WORD_SIZE);
    memcpy(top, other, WORD_SIZE);
    memcpy(other, temp, WORD_SIZE);

    return {1, 3};
}

static OpResult op_stub(ExecutionContext* ctx) {
    return {1, 3};
}

static OpResult op_invalid(ExecutionContext* ctx) {
    ctx->frame->state = 4;
    ctx->frame->halt_reason = 2;
    return {-1, 0};
}

// ===== PUSH/DUP/SWAP WRAPPERS =====

#define PUSH_HANDLER(n) static OpResult op_push##n(ExecutionContext* ctx) { return op_push_n(ctx, n); }
PUSH_HANDLER(1)  PUSH_HANDLER(2)  PUSH_HANDLER(3)  PUSH_HANDLER(4)
PUSH_HANDLER(5)  PUSH_HANDLER(6)  PUSH_HANDLER(7)  PUSH_HANDLER(8)
PUSH_HANDLER(9)  PUSH_HANDLER(10) PUSH_HANDLER(11) PUSH_HANDLER(12)
PUSH_HANDLER(13) PUSH_HANDLER(14) PUSH_HANDLER(15) PUSH_HANDLER(16)
PUSH_HANDLER(17) PUSH_HANDLER(18) PUSH_HANDLER(19) PUSH_HANDLER(20)
PUSH_HANDLER(21) PUSH_HANDLER(22) PUSH_HANDLER(23) PUSH_HANDLER(24)
PUSH_HANDLER(25) PUSH_HANDLER(26) PUSH_HANDLER(27) PUSH_HANDLER(28)
PUSH_HANDLER(29) PUSH_HANDLER(30) PUSH_HANDLER(31) PUSH_HANDLER(32)

#define DUP_HANDLER(n) static OpResult op_dup##n(ExecutionContext* ctx) { return op_dup_n(ctx, n); }
DUP_HANDLER(1)  DUP_HANDLER(2)  DUP_HANDLER(3)  DUP_HANDLER(4)
DUP_HANDLER(5)  DUP_HANDLER(6)  DUP_HANDLER(7)  DUP_HANDLER(8)
DUP_HANDLER(9)  DUP_HANDLER(10) DUP_HANDLER(11) DUP_HANDLER(12)
DUP_HANDLER(13) DUP_HANDLER(14) DUP_HANDLER(15) DUP_HANDLER(16)

#define SWAP_HANDLER(n) static OpResult op_swap##n(ExecutionContext* ctx) { return op_swap_n(ctx, n); }
SWAP_HANDLER(1)  SWAP_HANDLER(2)  SWAP_HANDLER(3)  SWAP_HANDLER(4)
SWAP_HANDLER(5)  SWAP_HANDLER(6)  SWAP_HANDLER(7)  SWAP_HANDLER(8)
SWAP_HANDLER(9)  SWAP_HANDLER(10) SWAP_HANDLER(11) SWAP_HANDLER(12)
SWAP_HANDLER(13) SWAP_HANDLER(14) SWAP_HANDLER(15) SWAP_HANDLER(16)

typedef OpResult (*OpHandler)(ExecutionContext*);

static const OpHandler JUMP_TABLE[256] = {
    op_stop,    op_add,     op_mul,     op_sub,     op_div,     op_stub,    op_mod,     op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_lt,      op_gt,      op_stub,    op_stub,    op_eq,      op_iszero,  op_and,     op_or,
    op_xor,     op_not,     op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_sload,   op_sstore,  op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_pop,     op_mload,   op_mstore,  op_mstore8, op_stub,    op_stub,    op_jump,    op_jumpi,
    op_pc,      op_stub,    op_gas,     op_jumpdest,op_stub,    op_stub,    op_stub,    op_push0,
    op_push1,   op_push2,   op_push3,   op_push4,   op_push5,   op_push6,   op_push7,   op_push8,
    op_push9,   op_push10,  op_push11,  op_push12,  op_push13,  op_push14,  op_push15,  op_push16,
    op_push17,  op_push18,  op_push19,  op_push20,  op_push21,  op_push22,  op_push23,  op_push24,
    op_push25,  op_push26,  op_push27,  op_push28,  op_push29,  op_push30,  op_push31,  op_push32,
    op_dup1,    op_dup2,    op_dup3,    op_dup4,    op_dup5,    op_dup6,    op_dup7,    op_dup8,
    op_dup9,    op_dup10,   op_dup11,   op_dup12,   op_dup13,   op_dup14,   op_dup15,   op_dup16,
    op_swap1,   op_swap2,   op_swap3,   op_swap4,   op_swap5,   op_swap6,   op_swap7,   op_swap8,
    op_swap9,   op_swap10,  op_swap11,  op_swap12,  op_swap13,  op_swap14,  op_swap15,  op_swap16,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,
    op_stub,    op_stub,    op_stub,    op_stub,    op_stub,    op_invalid, op_invalid, op_invalid
};

// ===== MAIN EXECUTION LOOP =====

void execute_message(MessageFrameMemory* frame, TracerCallbacks* tracer) {
    if (!frame) return;

    frame->state = 1; // CODE_EXECUTING

    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    ExecutionContext ctx = {
        frame,
        base + frame->stack_ptr,
        base + frame->memory_ptr,
        base + frame->code_ptr,
        reinterpret_cast<StorageEntry*>(base + frame->storage_ptr)
    };

    bool has_tracer = (tracer != nullptr && tracer->trace_pre_execution != nullptr);

    while (frame->pc < static_cast<int32_t>(frame->code_size) && frame->state == 1) {
        if (frame->gas_remaining < 3) {
            frame->state = 4;
            frame->halt_reason = 1;
            return;
        }

        uint8_t opcode = ctx.code[frame->pc];

        if (has_tracer) {
            tracer->trace_pre_execution(frame);
        }

        OpResult result = JUMP_TABLE[opcode](&ctx);

        if (result.pc_increment < 0) {
            if (frame->state == 1) {
                frame->state = 4;
                frame->halt_reason = 4;
            }
            return;
        }

        if (frame->gas_remaining < result.gas_cost) {
            frame->state = 4;
            frame->halt_reason = 1;
            return;
        }

        frame->gas_remaining -= result.gas_cost;

        if (has_tracer) {
            OperationResult op_result;
            op_result.gas_cost = result.gas_cost;
            op_result.halt_reason = 0;
            op_result.pc_increment = result.pc_increment;
            tracer->trace_post_execution(frame, &op_result);
        }

        if (result.pc_increment > 0) {
            frame->pc += result.pc_increment;
        }
    }

    if (frame->state == 1) {
        frame->state = 7;
    }
}

} // extern "C"
