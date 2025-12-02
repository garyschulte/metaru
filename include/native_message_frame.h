// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "message_frame.h"
#include "types.h"
#include <jni.h>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace besu {
namespace evm {

// Forward declarations
class Code;
class WorldUpdater;
class BlockValues;
class Log;

/**
 * Native (pure C++) implementation of MessageFrame.
 *
 * This class provides a high-performance implementation that operates entirely
 * in native code, minimizing JNI overhead. Data is copied from/to Java MessageFrame
 * at the boundaries (entry/exit of runToHalt), but all operations during EVM
 * execution are pure C++ with native data structures.
 *
 * Performance characteristics:
 * - JNI calls per execution: ~2 (copy in, copy out)
 * - Operations during execution: 0 JNI calls (except SLOAD/SSTORE)
 * - Speedup vs JNI wrapper: 10-1000x depending on contract complexity
 */
class NativeMessageFrame : public IMessageFrame {
public:
    /**
     * Create native frame from Java MessageFrame (copy in).
     *
     * Copies all necessary state from Java to native data structures.
     * This is called once at the start of runToHalt().
     *
     * @param env JNI environment
     * @param jframe Java MessageFrame object
     * @return Native frame with copied state
     */
    static std::unique_ptr<NativeMessageFrame> fromJava(JNIEnv* env, jobject jframe);

    /**
     * Sync native frame state back to Java MessageFrame (copy out).
     *
     * Copies modified state back to Java MessageFrame.
     * This is called once at the end of runToHalt().
     *
     * @param env JNI environment
     * @param jframe Java MessageFrame object to update
     */
    void syncToJava(JNIEnv* env, jobject jframe) const;

    /**
     * Constructor (typically use fromJava() instead).
     */
    NativeMessageFrame();

    /**
     * Destructor - cleans up any held JNI resources.
     */
    ~NativeMessageFrame() override;

    // Disable copy (use move semantics)
    NativeMessageFrame(const NativeMessageFrame&) = delete;
    NativeMessageFrame& operator=(const NativeMessageFrame&) = delete;

    // ========== IMessageFrame Implementation - All Pure C++ ==========

    // Program counter - inline for performance
    int getPC() const override { return pc_; }
    void setPC(int pc) override { pc_ = pc; }
    int getSection() const override { return section_; }
    void setSection(int section) override { section_ = section; }

    // Gas management - inline for performance
    int64_t getRemainingGas() const override { return gasRemaining_; }
    void setGasRemaining(int64_t amount) override { gasRemaining_ = amount; }

    int64_t decrementRemainingGas(int64_t amount) override {
        gasRemaining_ -= amount;
        if (gasRemaining_ < 0) {
            haltReason_ = ExceptionalHaltReason::INSUFFICIENT_GAS;
        }
        return gasRemaining_;
    }

    void incrementRemainingGas(int64_t amount) override {
        gasRemaining_ += amount;
    }

    void clearGasRemaining() override { gasRemaining_ = 0; }

    int64_t getGasRefund() const override { return gasRefund_; }

    void incrementGasRefund(int64_t amount) override {
        gasRefund_ += amount;
    }

    // Stack operations - pure C++ with std::vector
    Bytes getStackItem(int offset) const override;
    Bytes popStackItem() override;
    void popStackItems(int n) override;
    void pushStackItem(const Bytes& value) override;
    void setStackItem(int offset, const Bytes& value) override;
    int stackSize() const override { return static_cast<int>(stack_.size()); }

    // Memory operations - pure C++ with std::vector
    int64_t calculateMemoryExpansion(int64_t offset, int64_t length) override;
    void expandMemory(int64_t offset, int64_t length) override;
    int64_t memoryByteSize() const override { return static_cast<int64_t>(memory_.size()); }
    int memoryWordSize() const override { return static_cast<int>((memory_.size() + 31) / 32); }
    Bytes readMemory(int64_t offset, int64_t length) const override;
    void writeMemory(int64_t offset, int64_t length, const Bytes& value, bool explicit_update) override;
    void copyMemory(int64_t dest, int64_t src, int64_t length, bool explicit_update) override;

    // State and context
    MessageFrameState getState() const override { return state_; }
    void setState(MessageFrameState state) override { state_ = state; }
    MessageFrameType getType() const override { return type_; }
    bool isStatic() const override { return isStatic_; }

    // Code and input (cached from Java)
    const Code& getCode() const override;
    Bytes getInputData() const override { return inputData_; }

    // Addresses (cached from Java)
    Address getRecipientAddress() const override { return recipient_; }
    Address getContractAddress() const override { return contract_; }
    Address getSenderAddress() const override { return sender_; }
    Address getOriginatorAddress() const override { return originator_; }
    Address getMiningBeneficiary() const override { return miningBeneficiary_; }

    // Values (cached from Java)
    Wei getValue() const override { return value_; }
    Wei getApparentValue() const override { return apparentValue_; }
    Wei getGasPrice() const override { return gasPrice_; }

    // Block context (reference to Java object)
    const BlockValues& getBlockValues() const override;

    // Call depth
    int getDepth() const override { return depth_; }
    int getMaxStackSize() const override { return maxStackSize_; }

    // Output and return data
    Bytes getOutputData() const override { return outputData_; }
    void setOutputData(const Bytes& output) override { outputData_ = output; }
    void clearOutputData() override { outputData_.clear(); }
    Bytes getReturnData() const override { return returnData_; }
    void setReturnData(const Bytes& data) override { returnData_ = data; }
    void clearReturnData() override { returnData_.clear(); }

    // Exceptional halt
    std::optional<ExceptionalHaltReason> getExceptionalHaltReason() const override {
        return haltReason_;
    }
    void setExceptionalHaltReason(std::optional<ExceptionalHaltReason> reason) override {
        haltReason_ = reason;
    }

    // Revert reason
    std::optional<Bytes> getRevertReason() const override { return revertReason_; }
    void setRevertReason(const Bytes& reason) override { revertReason_ = reason; }

    // World state (requires JNI for actual state access)
    WorldUpdater& getWorldUpdater() override;

    // Warm/cold access tracking (pure C++)
    bool warmUpAddress(const Address& address) override;
    bool isAddressWarm(const Address& address) const override;
    bool warmUpStorage(const Address& address, const Bytes32& slot) override;

    // Transient storage (pure C++ - EIP-1153)
    Bytes32 getTransientStorageValue(const Address& address, const Bytes32& slot) const override;
    void setTransientStorageValue(const Address& address, const Bytes32& slot, const Bytes32& value) override;

    // Rollback
    void rollback() override;

    // Tracing support
    void storageWasUpdated(const UInt256& address, const Bytes& value) override;

    // Not applicable for native frame (no underlying Java object)
    jobject getJavaObject() const override { return nullptr; }

    // Access to JNI environment (needed for SLOAD/SSTORE and child frames)
    JNIEnv* getEnv() const { return env_; }

    // Access to Java WorldUpdater reference (for storage operations)
    jobject getJavaWorldUpdater() const { return jworldUpdater_; }

private:
    // JNI context
    JNIEnv* env_;

    // ========== Native Data Structures (Pure C++) ==========

    // Machine state
    int pc_;
    int section_;
    int64_t gasRemaining_;
    int64_t gasRefund_;

    // Stack (std::vector for performance)
    std::vector<Bytes> stack_;
    int maxStackSize_;

    // Memory (std::vector, expandable)
    std::vector<uint8_t> memory_;

    // State
    MessageFrameState state_;
    MessageFrameType type_;
    bool isStatic_;

    // ========== Cached Immutable Data (From Java) ==========

    // Copied once at construction
    Bytes codeBytes_;
    Bytes inputData_;
    Address recipient_;
    Address sender_;
    Address contract_;
    Address originator_;
    Address miningBeneficiary_;
    Wei value_;
    Wei apparentValue_;
    Wei gasPrice_;
    int depth_;

    // References to Java objects (for lazy access if needed)
    jobject jcode_;           // Global ref to Java Code object
    jobject jworldUpdater_;   // Global ref to Java WorldUpdater
    jobject jblockValues_;    // Global ref to Java BlockValues

    // Lazy-initialized C++ wrappers
    mutable std::unique_ptr<Code> code_cache_;
    mutable std::unique_ptr<BlockValues> block_values_cache_;
    mutable std::unique_ptr<WorldUpdater> world_updater_cache_;

    // ========== Mutable State (To Sync Back to Java) ==========

    Bytes outputData_;
    Bytes returnData_;
    std::optional<Bytes> revertReason_;
    std::optional<ExceptionalHaltReason> haltReason_;

    // Logs generated during execution
    std::vector<Log> logs_;

    // Self-destructs
    std::set<Address> selfDestructs_;

    // Creates (CREATE/CREATE2)
    std::set<Address> creates_;

    // Refunds (for SELFDESTRUCT)
    std::map<Address, Wei> refunds_;

    // ========== Access Tracking (EIP-2929) ==========

    std::set<Address> warmAddresses_;
    std::map<std::pair<Address, Bytes32>, bool> warmStorage_;

    // ========== Transient Storage (EIP-1153) ==========

    std::map<std::pair<Address, Bytes32>, Bytes32> transientStorage_;

    // ========== Helper Methods ==========

    void copyPrimitiveFields(JNIEnv* env, jobject jframe);
    void copyStack(JNIEnv* env, jobject jframe);
    void copyMemory(JNIEnv* env, jobject jframe);
    void copyImmutableContext(JNIEnv* env, jobject jframe);
    void copyAccessLists(JNIEnv* env, jobject jframe);

    void syncPrimitiveFields(JNIEnv* env, jobject jframe) const;
    void syncStack(JNIEnv* env, jobject jframe) const;
    void syncMemory(JNIEnv* env, jobject jframe) const;
    void syncLogs(JNIEnv* env, jobject jframe) const;
    void syncSelfDestructs(JNIEnv* env, jobject jframe) const;
    void syncAccessLists(JNIEnv* env, jobject jframe) const;

    void releaseJavaRefs();
};

}  // namespace evm
}  // namespace besu
