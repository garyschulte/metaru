// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "message_frame.h"
#include <optional>
#include <string>

namespace besu {
namespace evm {

/**
 * Result of an operation execution.
 * Corresponds to org.hyperledger.besu.evm.operation.Operation.OperationResult in Java.
 */
struct OperationResult {
    int64_t gasCost;
    std::optional<ExceptionalHaltReason> haltReason;
    int pcIncrement;

    OperationResult(int64_t gas, std::optional<ExceptionalHaltReason> halt = std::nullopt, int pc = 1)
        : gasCost(gas), haltReason(halt), pcIncrement(pc) {}

    bool isExceptional() const { return haltReason.has_value(); }
};

/**
 * Interface for EVM operations (opcodes).
 * Corresponds to org.hyperledger.besu.evm.operation.Operation in Java.
 */
class IOperation {
public:
    virtual ~IOperation() = default;

    /**
     * Execute the operation.
     * @param frame The message frame
     * @return The operation result
     */
    virtual OperationResult execute(IMessageFrame& frame) = 0;

    /**
     * Get the opcode value.
     * @return The opcode (0x00 - 0xFF)
     */
    virtual uint8_t getOpcode() const = 0;

    /**
     * Get the operation name.
     * @return The name (e.g., "ADD", "MUL", "SSTORE")
     */
    virtual const char* getName() const = 0;

    /**
     * Get the number of stack items consumed.
     * @return Number of items popped from stack
     */
    virtual int getStackItemsConsumed() const = 0;

    /**
     * Get the number of stack items produced.
     * @return Number of items pushed to stack
     */
    virtual int getStackItemsProduced() const = 0;

    /**
     * Check if this is a virtual operation (not a real opcode).
     * @return true if virtual
     */
    virtual bool isVirtualOperation() const { return false; }
};

/**
 * Operation registry for dispatching opcodes to operations.
 * Corresponds to org.hyperledger.besu.evm.operation.OperationRegistry in Java.
 */
class OperationRegistry {
public:
    OperationRegistry();
    ~OperationRegistry();

    /**
     * Register an operation.
     * @param operation The operation to register
     */
    void registerOperation(IOperation* operation);

    /**
     * Get operation by opcode.
     * @param opcode The opcode (0x00 - 0xFF)
     * @return The operation, or nullptr if not registered
     */
    IOperation* getOperation(uint8_t opcode) const;

    /**
     * Check if an opcode is registered.
     * @param opcode The opcode
     * @return true if registered
     */
    bool hasOperation(uint8_t opcode) const;

private:
    static constexpr size_t OPCODE_COUNT = 256;
    IOperation* operations_[OPCODE_COUNT];
};

}  // namespace evm
}  // namespace besu
