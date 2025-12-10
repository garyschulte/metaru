// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

/**
 * Block Execution Demo - Bare Metal RISC-V Version
 *
 * This version eliminates all C++ standard library dependencies (vector, string)
 * to work in freestanding/bare metal environments.
 *
 * Demonstrates complete EVM block execution using the witness architecture:
 * 1. Mock block with transactions
 * 2. Pre-loaded witness (accounts, storage, code)
 * 3. Execute transactions via optimized EVM
 * 4. Update state and finalize block
 */

#include <cstdio>
#include <cstring>
#include <cstdint>

#include "../include/message_frame_memory.h"
#include "../include/storage_memory.h"
#include "../include/account_witness.h"

// Include EVM implementation directly (no shared library in bare metal)
#include "../src/evm_optimized.cpp"

using namespace besu::evm;

// ========== Mock Data Structures (No std::vector) ==========

#define MAX_TX_DATA_SIZE 1024
#define MAX_TRANSACTIONS 10

struct Transaction {
    uint8_t from[20];
    uint8_t to[20];
    uint8_t value[32];
    uint8_t data[MAX_TX_DATA_SIZE];
    uint32_t data_size;
    uint64_t gas_limit;
    uint8_t gas_price[32];
};

struct Block {
    uint32_t number;
    uint8_t coinbase[20];
    uint64_t gas_limit;
    uint64_t timestamp;
    Transaction transactions[MAX_TRANSACTIONS];
    uint32_t tx_count;
};

// ========== Helper Functions ==========

void print_address(const char* label, const uint8_t* addr) {
    printf("%s: 0x", label);
    for (int i = 0; i < 20; i++) {
        printf("%02x", addr[i]);
    }
    printf("\n");
}

void print_u256(const char* label, const uint8_t* value) {
    printf("%s: 0x", label);
    bool started = false;
    for (int i = 0; i < 32; i++) {
        if (value[i] != 0 || started) {
            printf("%02x", value[i]);
            started = true;
        }
    }
    if (!started) printf("00");
    printf("\n");
}

void set_address(uint8_t* dest, const char* hex) {
    // Simple hex string to address converter (expects 40 hex chars)
    for (int i = 0; i < 20; i++) {
        unsigned int byte;
        sscanf(hex + i*2, "%2x", &byte);
        dest[i] = (uint8_t)byte;
    }
}

void set_u256(uint8_t* dest, uint64_t value) {
    memset(dest, 0, 32);
    // Big-endian encoding
    for (int i = 0; i < 8; i++) {
        dest[31 - i] = (value >> (i * 8)) & 0xFF;
    }
}

// ========== Mock Block Creation ==========

void create_mock_block(Block* block) {
    block->number = 12345;
    block->gas_limit = 30000000;
    block->timestamp = 1699999999;
    block->tx_count = 0;

    // Coinbase (miner address)
    set_address(block->coinbase, "1111111111111111111111111111111111111111");

    // Transaction 1: Simple value transfer
    Transaction* tx1 = &block->transactions[block->tx_count++];
    set_address(tx1->from, "1000000000000000000000000000000000000001");
    set_address(tx1->to, "2000000000000000000000000000000000000002");
    set_u256(tx1->value, 1000000000000000000ULL); // 1 ETH
    tx1->gas_limit = 21000;
    set_u256(tx1->gas_price, 20000000000ULL); // 20 Gwei
    tx1->data_size = 0;

    // Transaction 2: Contract call (PUSH1 5, PUSH1 10, ADD, STOP)
    Transaction* tx2 = &block->transactions[block->tx_count++];
    set_address(tx2->from, "1000000000000000000000000000000000000001");
    set_address(tx2->to, "3000000000000000000000000000000000000003");
    set_u256(tx2->value, 0);
    tx2->data[0] = 0x60;  // PUSH1
    tx2->data[1] = 0x05;  // 5
    tx2->data[2] = 0x60;  // PUSH1
    tx2->data[3] = 0x0a;  // 10
    tx2->data[4] = 0x01;  // ADD
    tx2->data[5] = 0x00;  // STOP
    tx2->data_size = 6;
    tx2->gas_limit = 100000;
    set_u256(tx2->gas_price, 20000000000ULL);
}

// ========== Witness Building ==========

#define MAX_WITNESS_SIZE (64 + (128 * 20) + (124 * 100))  // header + accounts + storage

struct WitnessMemory {
    uint8_t data[MAX_WITNESS_SIZE];
    TransactionWitness* header;
    AccountEntry* accounts;
    StorageEntry* storage;

    void init(size_t account_count, size_t storage_count) {
        // Calculate sizes
        size_t header_size = 64;
        size_t accounts_size = account_count * 128;
        size_t storage_size = storage_count * 124;

        // Zero memory
        memset(data, 0, sizeof(data));

        // Setup pointers
        header = reinterpret_cast<TransactionWitness*>(data);
        accounts = reinterpret_cast<AccountEntry*>(data + header_size);
        storage = reinterpret_cast<StorageEntry*>(data + header_size + accounts_size);

        // Initialize header
        header->account_count = 0;
        header->max_accounts = account_count;
        header->accounts_ptr = header_size;
        header->storage_count = 0;
        header->max_storage = storage_count;
        header->storage_ptr = header_size + accounts_size;
    }
};

void build_block_witness(const Block* block, WitnessMemory* witness) {
    // Pre-allocate space for all accounts that might be touched
    size_t account_count = block->tx_count * 3 + 1; // sender, recipient, contract per tx + coinbase
    size_t storage_count = 100; // Generous allocation for storage slots

    witness->init(account_count, storage_count);

    printf("\n=== Building Block Witness ===\n");

    // Add coinbase account
    AccountEntry* coinbase = &witness->accounts[witness->header->account_count++];
    memcpy(coinbase->address, block->coinbase, 20);
    set_u256(coinbase->balance, 1000000000000000000ULL); // 1 ETH initial
    coinbase->nonce = 0;
    coinbase->is_warm = 1;
    print_address("  Coinbase", coinbase->address);

    // Add transaction senders and recipients
    for (uint32_t i = 0; i < block->tx_count; i++) {
        const Transaction* tx = &block->transactions[i];

        // Add sender
        AccountEntry* sender = &witness->accounts[witness->header->account_count++];
        memcpy(sender->address, tx->from, 20);
        set_u256(sender->balance, 10000000000000000000ULL); // 10 ETH
        sender->nonce = 0;
        sender->is_warm = 1;
        print_address("  Sender", sender->address);

        // Add recipient
        AccountEntry* recipient = &witness->accounts[witness->header->account_count++];
        memcpy(recipient->address, tx->to, 20);
        set_u256(recipient->balance, 0);
        recipient->nonce = 0;
        recipient->code_size = 0;
        recipient->is_warm = 0; // Cold until accessed
        print_address("  Recipient", recipient->address);
    }

    printf("  Total accounts: %u\n", witness->header->account_count);
    printf("  Storage slots: %u\n", witness->header->storage_count);
}

// ========== Frame Memory Creation ==========

#define MAX_FRAME_SIZE (384 + (1024 * 32) + 1024 + MAX_TX_DATA_SIZE)

struct FrameMemory {
    uint8_t data[MAX_FRAME_SIZE];
    MessageFrameMemory* frame;
    uint8_t* stack;
    uint8_t* memory;
    uint8_t* code;

    void init(const Transaction* tx, const WitnessMemory* witness) {
        // Calculate sizes
        size_t header_size = 384;
        size_t stack_size = 1024 * 32;
        size_t memory_size = 1024;
        size_t code_size = tx->data_size;

        memset(data, 0, sizeof(data));

        // Setup pointers
        frame = reinterpret_cast<MessageFrameMemory*>(data);
        stack = data + header_size;
        memory = stack + stack_size;
        code = memory + memory_size;

        // Initialize frame header
        frame->pc = 0;
        frame->section = 0;
        frame->gas_remaining = tx->gas_limit;
        frame->gas_refund = 0;
        frame->stack_size = 0;
        frame->memory_size = 0;
        frame->state = 0; // NOT_STARTED
        frame->type = 0; // MESSAGE_CALL
        frame->is_static = 0;
        frame->depth = 0;

        // Set pointers (relative to frame start)
        frame->stack_ptr = header_size;
        frame->memory_ptr = header_size + stack_size;
        frame->code_ptr = header_size + stack_size + memory_size;
        frame->witness_ptr = reinterpret_cast<uintptr_t>(witness->data) -
                             reinterpret_cast<uintptr_t>(data);

        // Set sizes
        frame->code_size = code_size;

        // Copy code
        if (code_size > 0) {
            memcpy(code, tx->data, code_size);
        }

        // Set addresses
        memcpy(frame->recipient, tx->to, 20);
        memcpy(frame->sender, tx->from, 20);
        memcpy(frame->contract, tx->to, 20);
        memcpy(frame->originator, tx->from, 20);

        // Set value
        memcpy(frame->value, tx->value, 32);
    }
};

// ========== Block Execution ==========

void execute_block(Block* block) {
    printf("\n========================================\n");
    printf("=== Block #%u Execution ===\n", block->number);
    printf("========================================\n");

    // Build witness
    WitnessMemory witness;
    build_block_witness(block, &witness);

    printf("\n=== Executing Transactions ===\n");

    for (uint32_t i = 0; i < block->tx_count; i++) {
        const Transaction* tx = &block->transactions[i];

        printf("\n--- Transaction %u ---\n", i + 1);
        print_address("  From", tx->from);
        print_address("  To", tx->to);
        print_u256("  Value", tx->value);
        printf("  Gas limit: %lu\n", (unsigned long)tx->gas_limit);
        printf("  Data size: %u bytes\n", tx->data_size);

        // Create frame
        FrameMemory frame_mem;
        frame_mem.init(tx, &witness);

        // Execute
        printf("  Executing...\n");
        execute_message(frame_mem.frame, nullptr);

        // Check result
        const char* state_names[] = {
            "NOT_STARTED", "CODE_EXECUTING", "CODE_SUSPENDED",
            "CODE_SUCCESS", "EXCEPTIONAL_HALT", "REVERT",
            "INVALID", "COMPLETED_SUCCESS", "COMPLETED_FAILED"
        };

        int state_idx = frame_mem.frame->state;
        if (state_idx < 0 || state_idx > 8) state_idx = 0;

        printf("  Final state: %s\n", state_names[state_idx]);
        printf("  Gas remaining: %ld\n", frame_mem.frame->gas_remaining);
        printf("  Gas refund: %ld\n", frame_mem.frame->gas_refund);

        if (frame_mem.frame->state == 7) { // COMPLETED_SUCCESS
            printf("  ✓ Transaction succeeded\n");
        } else {
            printf("  ✗ Transaction failed\n");
        }
    }

    // Print final witness state
    printf("\n=== Final Block State ===\n");
    printf("Accounts:\n");
    for (uint32_t i = 0; i < witness.header->account_count; i++) {
        AccountEntry* acc = &witness.accounts[i];
        printf("  ");
        print_address("", acc->address);
        printf("    Balance: ");
        print_u256("", acc->balance);
        printf("    Nonce: %lu\n", (unsigned long)acc->nonce);
    }

    printf("\n========================================\n");
    printf("=== Block Execution Complete ===\n");
    printf("========================================\n");
}

// ========== Main ==========

int main() {
    printf("\n");
    printf("╔══════════════════════════════════════╗\n");
    printf("║   Besu Native EVM - RISC-V Demo    ║\n");
    printf("║   Bare Metal rv64im Target          ║\n");
    printf("╚══════════════════════════════════════╝\n");

    // Create mock block
    Block block;
    create_mock_block(&block);

    // Execute block
    execute_block(&block);

    printf("\nDemo complete!\n\n");

    return 0;
}
