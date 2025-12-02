// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "message_frame.h"
#include "operation.h"
#include "operation_tracer_jni.h"
#include <memory>

namespace besu {
namespace evm {

/**
 * Main EVM execution engine.
 * Corresponds to org.hyperledger.besu.evm.EVM in Java.
 */
class EVM {
public:
    /**
     * Constructor.
     * @param operations The operation registry
     */
    explicit EVM(std::unique_ptr<OperationRegistry> operations);

    /**
     * Destructor.
     */
    ~EVM();

    // Disable copy
    EVM(const EVM&) = delete;
    EVM& operator=(const EVM&) = delete;

    /**
     * Execute EVM code to halt.
     *
     * This is the main execution loop that processes bytecode until:
     * - Code completes successfully (RETURN, STOP)
     * - Code reverts (REVERT)
     * - An exceptional halt condition occurs (out of gas, invalid opcode, etc.)
     * - Code is suspended (CALL, CREATE - spawning child frame)
     *
     * @param frame The message frame containing execution context
     * @param tracer The operation tracer for debugging/profiling
     */
    void runToHalt(IMessageFrame& frame, IOperationTracer& tracer);

    /**
     * Get the operation registry.
     * @return The operation registry
     */
    const OperationRegistry& getOperations() const { return *operations_; }

private:
    std::unique_ptr<OperationRegistry> operations_;

    // Hot-path optimizations for common opcodes
    void executePushInline(IMessageFrame& frame, uint8_t opcode);
    void executeDupInline(IMessageFrame& frame, uint8_t opcode);
    void executeSwapInline(IMessageFrame& frame, uint8_t opcode);

    // Helper methods
    void validateStackForOperation(const IMessageFrame& frame, const IOperation& op);
    void updateProgramCounter(IMessageFrame& frame, const OperationResult& result);
};

}  // namespace evm
}  // namespace besu
