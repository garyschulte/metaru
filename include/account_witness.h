// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <cstring>

namespace besu {
namespace evm {

/**
 * Account witness for EVM execution.
 *
 * Pre-loads all account data that might be accessed during transaction execution.
 * This eliminates FFI callbacks to Java's WorldUpdater.
 *
 * The witness contains:
 * 1. Account entries: Basic account data (balance, nonce, code hash, exists)
 * 2. Code entries: Bytecode for contracts
 * 3. Storage entries: Storage slots (already defined in storage_memory.h)
 *
 * All data is pre-loaded before execution begins, keyed by address.
 */

/**
 * Account entry (128 bytes, cache-line aligned).
 *
 * Contains account state data needed for:
 * - BALANCE opcode
 * - EXTCODESIZE, EXTCODECOPY, EXTCODEHASH opcodes
 * - CALL family (balance checks, code execution)
 * - CREATE/CREATE2 (nonce, existence checks)
 *
 * IMPORTANT: Presence in witness = account exists
 * - If address found → account exists (even if balance=0, nonce=0, no code)
 * - If address NOT found → account does not exist
 * - No explicit exists field needed
 */
struct AccountEntry {
    uint8_t  address[20];       // Account address
    uint8_t  balance[32];       // Account balance (Wei, big-endian)
    uint64_t nonce;             // Account nonce
    uint8_t  code_hash[32];     // Keccak256 hash of code
    uint32_t code_size;         // Size of code in bytes
    uint64_t code_offset;       // Offset to code bytes in witness
    uint8_t  is_warm;           // 1 if warm (EIP-2929), 0 if cold
    uint8_t  padding[15];       // Align to 128 bytes
};

static_assert(sizeof(AccountEntry) == 128, "AccountEntry must be 128 bytes");

/**
 * Code entry for contract bytecode.
 *
 * Stores actual bytecode. Referenced by AccountEntry.code_offset.
 * Variable size, stored sequentially after account entries.
 */
struct CodeEntry {
    uint8_t  address[20];       // Account address (for lookup)
    uint32_t size;              // Code size in bytes
    uint8_t  padding[8];        // Align header to 32 bytes
    // Followed by code bytes (variable length)
};

/**
 * Complete transaction witness structure.
 *
 * Memory layout:
 * ┌─────────────────────────┐
 * │ AccountEntry[0]         │ 128 bytes
 * │ AccountEntry[1]         │ 128 bytes
 * │ ...                     │
 * │ AccountEntry[n-1]       │ 128 bytes
 * ├─────────────────────────┤
 * │ CodeEntry[0] header     │ 32 bytes
 * │   + code bytes          │ variable
 * │ CodeEntry[1] header     │ 32 bytes
 * │   + code bytes          │ variable
 * │ ...                     │
 * ├─────────────────────────┤
 * │ StorageEntry[0]         │ 124 bytes
 * │ StorageEntry[1]         │ 124 bytes
 * │ ...                     │
 * └─────────────────────────┘
 */
struct TransactionWitness {
    uint32_t account_count;     // Number of accounts in witness
    uint32_t max_accounts;      // Maximum accounts allocated
    uint64_t accounts_ptr;      // Offset to AccountEntry array

    uint32_t code_count;        // Number of code entries
    uint64_t codes_ptr;         // Offset to code section
    uint64_t codes_size;        // Total size of code section

    uint32_t storage_count;     // Number of storage entries
    uint32_t max_storage;       // Maximum storage entries allocated
    uint64_t storage_ptr;       // Offset to StorageEntry array
};

/**
 * Helper functions for account lookups.
 */
namespace witness {

/**
 * Find account entry by address.
 * Returns nullptr if not found.
 */
inline AccountEntry* find_account(AccountEntry* entries, uint32_t count,
                                   const uint8_t* address) {
    for (uint32_t i = 0; i < count; i++) {
        if (memcmp(entries[i].address, address, 20) == 0) {
            return &entries[i];
        }
    }
    return nullptr;
}

/**
 * Get code bytes for an account.
 * Returns pointer to code and sets size, or returns nullptr if not found.
 */
inline const uint8_t* get_code(const uint8_t* witness_base,
                                const AccountEntry* account,
                                uint32_t* out_size) {
    if (!account || account->code_size == 0) {
        if (out_size) *out_size = 0;
        return nullptr;
    }

    if (out_size) *out_size = account->code_size;
    return witness_base + account->code_offset;
}

/**
 * Check if address is empty account (EIP-161).
 * Empty = nonce == 0 && balance == 0 && code_size == 0
 */
inline bool is_empty_account(const AccountEntry* account) {
    if (!account) {
        return true;  // Not in witness = doesn't exist = empty
    }

    // Check nonce == 0
    if (account->nonce != 0) {
        return false;
    }

    // Check balance == 0 (32-byte big-endian)
    for (int i = 0; i < 32; i++) {
        if (account->balance[i] != 0) {
            return false;
        }
    }

    // Check code_size == 0
    if (account->code_size != 0) {
        return false;
    }

    return true;
}

/**
 * Check if account exists.
 * Returns true if account found in witness (even if empty).
 */
inline bool account_exists(const AccountEntry* account) {
    return account != nullptr;
}

/**
 * Add new account to witness (for CREATE/CREATE2 or value transfer to new address).
 * Returns pointer to new entry, or nullptr if max_accounts reached.
 */
inline AccountEntry* add_account(AccountEntry* entries, uint32_t* count,
                                 uint32_t max_accounts, const uint8_t* address) {
    if (*count >= max_accounts) {
        return nullptr;
    }

    AccountEntry* entry = &entries[*count];
    memcpy(entry->address, address, 20);
    memset(entry->balance, 0, 32);        // Zero balance initially
    entry->nonce = 0;                     // Nonce starts at 0
    memset(entry->code_hash, 0, 32);      // Empty code hash
    entry->code_size = 0;                 // No code
    entry->code_offset = 0;               // No code offset
    entry->is_warm = 1;                   // Newly created = warm
    (*count)++;
    return entry;
}

/**
 * Mark account as warm (EIP-2929).
 * Returns gas cost (cold=2600, warm=100).
 */
inline int mark_warm_account(AccountEntry* account) {
    if (!account) return 2600;  // Not in witness = cold

    int gas_cost = account->is_warm ? 100 : 2600;
    account->is_warm = 1;
    return gas_cost;
}

/**
 * Transfer value between accounts (for CALL with value).
 * Updates balances in witness.
 * Returns false if insufficient balance.
 */
inline bool transfer_value(AccountEntry* from, AccountEntry* to,
                            const uint8_t* value) {
    if (!from || !to) return false;

    // Check if value is zero (common case - skip transfer)
    bool is_zero = true;
    for (int i = 0; i < 32; i++) {
        if (value[i] != 0) {
            is_zero = false;
            break;
        }
    }
    if (is_zero) return true;

    // Check sufficient balance (compare big-endian)
    // This is a simplified check - real implementation needs proper 256-bit arithmetic
    for (int i = 0; i < 32; i++) {
        if (from->balance[i] < value[i]) {
            return false;  // Insufficient balance
        } else if (from->balance[i] > value[i]) {
            break;  // Definitely sufficient
        }
    }

    // Perform transfer (simplified - needs proper 256-bit arithmetic)
    // from->balance -= value
    // to->balance += value
    // TODO: Implement proper big-endian subtraction/addition

    return true;
}

/**
 * Increment account nonce.
 */
inline void increment_nonce(AccountEntry* account) {
    if (account) {
        account->nonce++;
    }
}

/**
 * Set account code (for CREATE/CREATE2).
 * Note: Code bytes must be written to witness code section separately.
 */
inline void set_account_code(AccountEntry* account, const uint8_t* code_hash,
                              uint32_t code_size, uint64_t code_offset) {
    if (account) {
        memcpy(account->code_hash, code_hash, 32);
        account->code_size = code_size;
        account->code_offset = code_offset;
    }
}

} // namespace witness

} // namespace evm
} // namespace besu
