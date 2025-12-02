// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <jni.h>
#include "types.h"
#include <memory>
#include <optional>

namespace besu {
namespace evm {

// Forward declarations
class Code;
class WorldUpdater;
class BlockValues;
class Operation;

/**
 * MessageFrame state enum.
 * Corresponds to org.hyperledger.besu.evm.frame.MessageFrame.State in Java.
 */
enum class MessageFrameState {
    NOT_STARTED,
    CODE_EXECUTING,
    CODE_SUCCESS,
    CODE_SUSPENDED,
    EXCEPTIONAL_HALT,
    REVERT,
    COMPLETED_FAILED,
    COMPLETED_SUCCESS
};

/**
 * MessageFrame type enum.
 * Corresponds to org.hyperledger.besu.evm.frame.MessageFrame.Type in Java.
 */
enum class MessageFrameType {
    CONTRACT_CREATION,
    MESSAGE_CALL
};

/**
 * Exceptional halt reasons.
 * Corresponds to org.hyperledger.besu.evm.frame.ExceptionalHaltReason in Java.
 */
enum class ExceptionalHaltReason {
    NONE,
    INSUFFICIENT_GAS,
    INVALID_OPERATION,
    INVALID_JUMP_DESTINATION,
    STACK_OVERFLOW,
    STACK_UNDERFLOW,
    ILLEGAL_STATE_CHANGE,
    OUT_OF_BOUNDS,
    CODE_TOO_LARGE,
    INVALID_CODE,
    PRECOMPILE_ERROR,
    TOO_MANY_STACK_ITEMS,
    INSUFFICIENT_STACK_ITEMS
};

/**
 * Interface for MessageFrame to support both JNI wrapper and pure C++ implementations.
 * Corresponds to org.hyperledger.besu.evm.frame.IMessageFrame in Java.
 */
class IMessageFrame {
public:
    virtual ~IMessageFrame() = default;

    // Program counter
    virtual int getPC() const = 0;
    virtual void setPC(int pc) = 0;
    virtual int getSection() const = 0;
    virtual void setSection(int section) = 0;

    // Gas management
    virtual int64_t getRemainingGas() const = 0;
    virtual void setGasRemaining(int64_t amount) = 0;
    virtual int64_t decrementRemainingGas(int64_t amount) = 0;
    virtual void incrementRemainingGas(int64_t amount) = 0;
    virtual void clearGasRemaining() = 0;
    virtual int64_t getGasRefund() const = 0;
    virtual void incrementGasRefund(int64_t amount) = 0;

    // Stack operations
    virtual Bytes getStackItem(int offset) const = 0;
    virtual Bytes popStackItem() = 0;
    virtual void popStackItems(int n) = 0;
    virtual void pushStackItem(const Bytes& value) = 0;
    virtual void setStackItem(int offset, const Bytes& value) = 0;
    virtual int stackSize() const = 0;

    // Memory operations
    virtual int64_t calculateMemoryExpansion(int64_t offset, int64_t length) = 0;
    virtual void expandMemory(int64_t offset, int64_t length) = 0;
    virtual int64_t memoryByteSize() const = 0;
    virtual int memoryWordSize() const = 0;
    virtual Bytes readMemory(int64_t offset, int64_t length) const = 0;
    virtual void writeMemory(int64_t offset, int64_t length, const Bytes& value, bool explicit_update) = 0;
    virtual void copyMemory(int64_t dest, int64_t src, int64_t length, bool explicit_update) = 0;

    // State and context
    virtual MessageFrameState getState() const = 0;
    virtual void setState(MessageFrameState state) = 0;
    virtual MessageFrameType getType() const = 0;
    virtual bool isStatic() const = 0;

    // Code and input
    virtual const Code& getCode() const = 0;
    virtual Bytes getInputData() const = 0;

    // Addresses
    virtual Address getRecipientAddress() const = 0;
    virtual Address getContractAddress() const = 0;
    virtual Address getSenderAddress() const = 0;
    virtual Address getOriginatorAddress() const = 0;
    virtual Address getMiningBeneficiary() const = 0;

    // Values
    virtual Wei getValue() const = 0;
    virtual Wei getApparentValue() const = 0;
    virtual Wei getGasPrice() const = 0;

    // Block context
    virtual const BlockValues& getBlockValues() const = 0;

    // Call depth
    virtual int getDepth() const = 0;
    virtual int getMaxStackSize() const = 0;

    // Output and return data
    virtual Bytes getOutputData() const = 0;
    virtual void setOutputData(const Bytes& output) = 0;
    virtual void clearOutputData() = 0;
    virtual Bytes getReturnData() const = 0;
    virtual void setReturnData(const Bytes& data) = 0;
    virtual void clearReturnData() = 0;

    // Exceptional halt
    virtual std::optional<ExceptionalHaltReason> getExceptionalHaltReason() const = 0;
    virtual void setExceptionalHaltReason(std::optional<ExceptionalHaltReason> reason) = 0;

    // Revert reason
    virtual std::optional<Bytes> getRevertReason() const = 0;
    virtual void setRevertReason(const Bytes& reason) = 0;

    // World state
    virtual WorldUpdater& getWorldUpdater() = 0;

    // Warm/cold access tracking (EIP-2929)
    virtual bool warmUpAddress(const Address& address) = 0;
    virtual bool isAddressWarm(const Address& address) const = 0;
    virtual bool warmUpStorage(const Address& address, const Bytes32& slot) = 0;

    // Transient storage (EIP-1153)
    virtual Bytes32 getTransientStorageValue(const Address& address, const Bytes32& slot) const = 0;
    virtual void setTransientStorageValue(const Address& address, const Bytes32& slot, const Bytes32& value) = 0;

    // Rollback
    virtual void rollback() = 0;

    // Tracing support
    virtual void storageWasUpdated(const UInt256& address, const Bytes& value) = 0;

    // Get underlying Java object (for JNI wrapper)
    virtual jobject getJavaObject() const = 0;
};

/**
 * MessageFrame implementation that wraps a Java MessageFrame via JNI.
 * All operations delegate to the Java object through JNI calls.
 */
class MessageFrameJNI : public IMessageFrame {
public:
    /**
     * Constructor.
     * @param env JNI environment
     * @param jframe Java MessageFrame object
     */
    MessageFrameJNI(JNIEnv* env, jobject jframe);

    /**
     * Destructor - releases global reference to Java frame.
     */
    ~MessageFrameJNI() override;

    // Disable copy
    MessageFrameJNI(const MessageFrameJNI&) = delete;
    MessageFrameJNI& operator=(const MessageFrameJNI&) = delete;

    // IMessageFrame implementation - all delegate to Java via JNI
    int getPC() const override;
    void setPC(int pc) override;
    int getSection() const override;
    void setSection(int section) override;

    int64_t getRemainingGas() const override;
    void setGasRemaining(int64_t amount) override;
    int64_t decrementRemainingGas(int64_t amount) override;
    void incrementRemainingGas(int64_t amount) override;
    void clearGasRemaining() override;
    int64_t getGasRefund() const override;
    void incrementGasRefund(int64_t amount) override;

    Bytes getStackItem(int offset) const override;
    Bytes popStackItem() override;
    void popStackItems(int n) override;
    void pushStackItem(const Bytes& value) override;
    void setStackItem(int offset, const Bytes& value) override;
    int stackSize() const override;

    int64_t calculateMemoryExpansion(int64_t offset, int64_t length) override;
    void expandMemory(int64_t offset, int64_t length) override;
    int64_t memoryByteSize() const override;
    int memoryWordSize() const override;
    Bytes readMemory(int64_t offset, int64_t length) const override;
    void writeMemory(int64_t offset, int64_t length, const Bytes& value, bool explicit_update) override;
    void copyMemory(int64_t dest, int64_t src, int64_t length, bool explicit_update) override;

    MessageFrameState getState() const override;
    void setState(MessageFrameState state) override;
    MessageFrameType getType() const override;
    bool isStatic() const override;

    const Code& getCode() const override;
    Bytes getInputData() const override;

    Address getRecipientAddress() const override;
    Address getContractAddress() const override;
    Address getSenderAddress() const override;
    Address getOriginatorAddress() const override;
    Address getMiningBeneficiary() const override;

    Wei getValue() const override;
    Wei getApparentValue() const override;
    Wei getGasPrice() const override;

    const BlockValues& getBlockValues() const override;

    int getDepth() const override;
    int getMaxStackSize() const override;

    Bytes getOutputData() const override;
    void setOutputData(const Bytes& output) override;
    void clearOutputData() override;
    Bytes getReturnData() const override;
    void setReturnData(const Bytes& data) override;
    void clearReturnData() override;

    std::optional<ExceptionalHaltReason> getExceptionalHaltReason() const override;
    void setExceptionalHaltReason(std::optional<ExceptionalHaltReason> reason) override;

    std::optional<Bytes> getRevertReason() const override;
    void setRevertReason(const Bytes& reason) override;

    WorldUpdater& getWorldUpdater() override;

    bool warmUpAddress(const Address& address) override;
    bool isAddressWarm(const Address& address) const override;
    bool warmUpStorage(const Address& address, const Bytes32& slot) override;

    Bytes32 getTransientStorageValue(const Address& address, const Bytes32& slot) const override;
    void setTransientStorageValue(const Address& address, const Bytes32& slot, const Bytes32& value) override;

    void rollback() override;

    void storageWasUpdated(const UInt256& address, const Bytes& value) override;

    jobject getJavaObject() const override { return jframe_; }

private:
    JNIEnv* env_;
    jobject jframe_;  // Global reference to Java MessageFrame

    // Cached Java objects (mutable for lazy initialization in const methods)
    mutable std::unique_ptr<Code> code_cache_;
    mutable std::unique_ptr<BlockValues> block_values_cache_;
    mutable std::unique_ptr<WorldUpdater> world_updater_cache_;
};

}  // namespace evm
}  // namespace besu
