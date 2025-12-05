// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstring>

namespace besu {
namespace evm {

/**
 * Flat storage structure for SLOAD/SSTORE operations across multiple accounts.
 *
 * PROBLEM: Ethereum has an accounts trie, and each account has its own storage trie.
 * A single transaction can touch multiple accounts and their storage.
 *
 * SOLUTION: Pre-load ALL potentially accessed storage slots from ALL accounts into
 * a flat array keyed by (address, slot). This eliminates FFI callbacks.
 *
 * Layout per storage entry:
 * - 20 bytes: account address
 * - 32 bytes: storage key (slot number)
 * - 32 bytes: current storage value
 * - 32 bytes: original value (for gas refunds - EIP-2200)
 * - 1 byte: is_warm flag (EIP-2929)
 * - 7 bytes: padding for alignment
 *
 * Total: 124 bytes per entry
 *
 * Lookup is O(n) linear search, but:
 * - Most transactions touch < 100 slots
 * - Linear search is cache-friendly
 * - Alternative (hash table) adds complexity without clear perf win for small N
 */

struct StorageEntry {
    uint8_t address[20];      // Account address (which account's storage)
    uint8_t key[32];          // Storage key (slot number within account)
    uint8_t value[32];        // Current storage value
    uint8_t original[32];     // Original value (for gas refunds)
    uint8_t is_warm;          // 1 if warm, 0 if cold (EIP-2929)
    uint8_t padding[7];       // Align to 8-byte boundary
};

static_assert(sizeof(StorageEntry) == 124, "StorageEntry must be 124 bytes");

/**
 * Helper functions for storage lookups.
 */
namespace storage {

/**
 * Find storage entry for a given address + key.
 * Returns nullptr if not found.
 */
inline StorageEntry* find(StorageEntry* entries, uint32_t count,
                          const uint8_t* address, const uint8_t* key) {
    for (uint32_t i = 0; i < count; i++) {
        // Check address (20 bytes) then key (32 bytes)
        if (memcmp(entries[i].address, address, 20) == 0 &&
            memcmp(entries[i].key, key, 32) == 0) {
            return &entries[i];
        }
    }
    return nullptr;
}

/**
 * Add a new storage entry (for SSTORE to previously unaccessed slot).
 * Returns nullptr if max_slots reached.
 */
inline StorageEntry* add(StorageEntry* entries, uint32_t* count, uint32_t max_slots,
                         const uint8_t* address, const uint8_t* key) {
    if (*count >= max_slots) {
        return nullptr;
    }

    StorageEntry* entry = &entries[*count];
    memcpy(entry->address, address, 20);
    memcpy(entry->key, key, 32);
    memset(entry->value, 0, 32);        // New slot = 0
    memset(entry->original, 0, 32);     // Original = 0
    entry->is_warm = 0;                  // Cold on first access
    (*count)++;
    return entry;
}

} // namespace storage

} // namespace evm
} // namespace besu
