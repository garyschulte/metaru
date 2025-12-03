// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace besu {
namespace evm {

/**
 * Memory layout for MessageFrame shared between Java and C++ via Panama FFM.
 *
 * CRITICAL: This struct MUST match MessageFrameLayout.java exactly!
 * Any changes must be synchronized between both definitions.
 *
 * Total size: 384 bytes for header + variable data
 *
 * Layout:
 * - Header (384 bytes): All metadata
 * - Stack (dynamic): stackSize * 32 bytes
 * - Memory (dynamic): memorySize bytes
 * - Code (dynamic): codeSize bytes
 * - Input (dynamic): inputSize bytes
 * - Output (dynamic): outputSize bytes
 * - Return data (dynamic): returnDataSize bytes
 * - Logs (dynamic): logCount * sizeof(Log)
 * - Access lists (dynamic)
 *
 * PORTABILITY NOTES:
 * - Endianness: Assumes little-endian (x86-64, aarch64 Linux/Darwin). Not tested on big-endian.
 * - Alignment: Struct is 64-byte aligned to match cache lines on both x86 and ARM.
 * - Signedness: Some fields are uint32_t (C++) but read as int32_t (Java). See field comments.
 * - GC Safety: Java uses Panama FFM Arena - memory is off-heap and pinned during native calls.
 */
struct __attribute__((aligned(64))) MessageFrameMemory {
    // ========== Machine State (48 bytes) ==========

    int32_t   pc;                  // Program counter
    int32_t   section;             // Code section (EOF support)
    int64_t   gas_remaining;       // Gas remaining
    int64_t   gas_refund;          // Gas refund amount
    int32_t   stack_size;          // Current stack size
    int32_t   memory_size;         // Current memory size in bytes (signed, max 2^31-1 = 2GB)
    uint32_t  state;               // MessageFrameState enum (as int)
    uint32_t  type;                // MessageFrameType enum (as int)
    uint32_t  is_static;           // Static call flag (0 or 1)
    uint32_t  depth;               // Call depth

    // ========== Pointers to Variable Data (64 bytes) ==========
    // These are relative offsets from the start of the MemorySegment

    uint64_t  stack_ptr;           // Offset to stack data
    uint64_t  memory_ptr;          // Offset to memory data
    uint64_t  code_ptr;            // Offset to code bytes
    uint64_t  input_ptr;           // Offset to input data
    uint64_t  output_ptr;          // Offset to output data
    uint64_t  return_data_ptr;     // Offset to return data
    uint64_t  logs_ptr;            // Offset to logs array
    uint64_t  warm_addresses_ptr;  // Offset to warm address set

    // ========== Sizes for Variable Data (32 bytes) ==========
    // IMPORTANT: Size fields are uint32_t (unsigned) in C++ but read as int32_t (signed) in Java.
    // To ensure Java compatibility, these values MUST NOT exceed 2^31-1 (2,147,483,647 bytes).
    // This is sufficient for EVM constraints:
    // - Code: max 24KB (EIP-170)
    // - Input: practical limit ~hundreds of KB
    // - Memory: gas costs limit to ~hundreds of MB
    // - Stack: max 1024 items = 32KB

    uint32_t  code_size;           // Code size in bytes (MUST be < 2^31 for Java)
    uint32_t  input_size;          // Input data size in bytes (MUST be < 2^31 for Java)
    uint32_t  output_size;         // Output data size in bytes (MUST be < 2^31 for Java)
    uint32_t  return_data_size;    // Return data size in bytes (MUST be < 2^31 for Java)
    uint32_t  logs_count;          // Number of logs
    uint32_t  warm_addresses_count; // Number of warm addresses
    uint32_t  warm_storage_count;  // Number of warm storage slots
    uint32_t  padding2;            // Padding

    // ========== Immutable Context - Addresses (100 bytes) ==========

    uint8_t   recipient[20];       // Recipient address
    uint8_t   sender[20];          // Sender address
    uint8_t   contract[20];        // Contract address
    uint8_t   originator[20];      // Transaction originator
    uint8_t   mining_beneficiary[20]; // Mining beneficiary (coinbase)

    // ========== Immutable Context - Values (96 bytes) ==========

    uint8_t   value[32];           // Wei value transferred
    uint8_t   apparent_value[32];  // Apparent value (for CALLCODE/DELEGATECALL)
    uint8_t   gas_price[32];       // Gas price in Wei

    // ========== Halt Reason (4 bytes) ==========

    uint32_t  halt_reason;         // ExceptionalHaltReason enum (0 = none)

    // ========== Reserved for Future Use (40 bytes) ==========

    uint8_t   reserved[40];        // Padding to 384 bytes total
};

// Static assertions to verify struct layout
static_assert(sizeof(MessageFrameMemory) == 384,
              "MessageFrameMemory must be exactly 384 bytes");

static_assert(offsetof(MessageFrameMemory, pc) == 0,
              "pc must be at offset 0");

static_assert(offsetof(MessageFrameMemory, stack_ptr) == 48,
              "stack_ptr must be at offset 48");

static_assert(offsetof(MessageFrameMemory, code_size) == 112,
              "code_size must be at offset 112");

static_assert(offsetof(MessageFrameMemory, recipient) == 144,
              "recipient must be at offset 144");

static_assert(offsetof(MessageFrameMemory, value) == 244,
              "value must be at offset 244");

static_assert(offsetof(MessageFrameMemory, halt_reason) == 340,
              "halt_reason must be at offset 340");

// Constants
constexpr size_t STACK_ITEM_SIZE = 32;
constexpr size_t MAX_STACK_SIZE = 1024;
constexpr size_t ADDRESS_SIZE = 20;
constexpr size_t WORD_SIZE = 32;

/**
 * Helper functions for working with MessageFrameMemory.
 */
namespace frame_memory {

/**
 * Get pointer to stack item at given index.
 * @param frame The frame memory
 * @param index Stack index (0 = top)
 * @return Pointer to 32-byte stack item
 */
inline uint8_t* getStackItem(MessageFrameMemory* frame, int index) {
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    return base + frame->stack_ptr + (index * STACK_ITEM_SIZE);
}

/**
 * Get pointer to memory at given offset.
 * @param frame The frame memory
 * @param offset Memory offset
 * @return Pointer to memory byte
 */
inline uint8_t* getMemory(MessageFrameMemory* frame, uint64_t offset) {
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    return base + frame->memory_ptr + offset;
}

/**
 * Get pointer to code bytes.
 * @param frame The frame memory
 * @return Pointer to code
 */
inline const uint8_t* getCode(const MessageFrameMemory* frame) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(frame);
    return base + frame->code_ptr;
}

/**
 * Get pointer to input data.
 * @param frame The frame memory
 * @return Pointer to input
 */
inline const uint8_t* getInput(const MessageFrameMemory* frame) {
    const uint8_t* base = reinterpret_cast<const uint8_t*>(frame);
    return base + frame->input_ptr;
}

/**
 * Set output data.
 * @param frame The frame memory
 * @param data Output data
 * @param size Output size
 */
inline void setOutput(MessageFrameMemory* frame, const uint8_t* data, uint32_t size) {
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    uint8_t* output = base + frame->output_ptr;
    std::memcpy(output, data, size);
    frame->output_size = size;
}

/**
 * Set return data.
 * @param frame The frame memory
 * @param data Return data
 * @param size Return data size
 */
inline void setReturnData(MessageFrameMemory* frame, const uint8_t* data, uint32_t size) {
    uint8_t* base = reinterpret_cast<uint8_t*>(frame);
    uint8_t* returnData = base + frame->return_data_ptr;
    std::memcpy(returnData, data, size);
    frame->return_data_size = size;
}

} // namespace frame_memory

} // namespace evm
} // namespace besu
