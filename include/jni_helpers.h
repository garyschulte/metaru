// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <jni.h>
#include "types.h"
#include <functional>
#include <optional>
#include <string>

namespace besu {
namespace evm {
namespace jni {

/**
 * RAII wrapper for JNI local frame management.
 * Automatically pushes a local frame on construction and pops it on destruction.
 */
class LocalFrame {
public:
    explicit LocalFrame(JNIEnv* env, jint capacity = 16);
    ~LocalFrame();

    LocalFrame(const LocalFrame&) = delete;
    LocalFrame& operator=(const LocalFrame&) = delete;

private:
    JNIEnv* env_;
};

/**
 * RAII wrapper for JNI critical section (GetPrimitiveArrayCritical).
 */
template<typename T>
class CriticalArray {
public:
    CriticalArray(JNIEnv* env, jarray array, bool isCopy = false);
    ~CriticalArray();

    CriticalArray(const CriticalArray&) = delete;
    CriticalArray& operator=(const CriticalArray&) = delete;

    T* get() const { return ptr_; }
    jsize length() const { return length_; }

private:
    JNIEnv* env_;
    jarray array_;
    T* ptr_;
    jsize length_;
};

/**
 * Exception handling utilities for JNI.
 */
namespace exception {

/**
 * Check if a Java exception is pending and clear it.
 * @return true if an exception was pending
 */
bool checkAndClear(JNIEnv* env);

/**
 * Check if a Java exception is pending and throw a C++ exception.
 * @throws std::runtime_error if Java exception pending
 */
void checkAndThrow(JNIEnv* env, const char* context = nullptr);

/**
 * Throw a Java exception from C++.
 * @param className Fully qualified Java exception class name
 * @param message Exception message
 */
void throwJava(JNIEnv* env, const char* className, const char* message);

/**
 * Throw a Java RuntimeException.
 */
void throwRuntimeException(JNIEnv* env, const char* message);

/**
 * Throw a Java IllegalArgumentException.
 */
void throwIllegalArgumentException(JNIEnv* env, const char* message);

/**
 * Throw a Java IllegalStateException.
 */
void throwIllegalStateException(JNIEnv* env, const char* message);

}  // namespace exception

/**
 * JNI method and field ID cache for performance.
 * Caches method and field IDs to avoid repeated lookups.
 */
class JniCache {
public:
    explicit JniCache(JNIEnv* env);
    ~JniCache() = default;

    // Initialize all cached IDs
    void initialize(JNIEnv* env);

    // Java class references (global refs)
    jclass bytesClass;
    jclass addressClass;
    jclass weiClass;
    jclass uint256Class;
    jclass optionalClass;
    jclass messageFrameClass;
    jclass operationTracerClass;
    jclass operationClass;
    jclass operationResultClass;
    jclass exceptionalHaltReasonClass;
    jclass codeClass;
    jclass worldUpdaterClass;
    jclass blockValuesClass;

    // org.apache.tuweni.bytes.Bytes methods
    jmethodID bytesWrap;
    jmethodID bytesToArray;
    jmethodID bytesSize;

    // org.hyperledger.besu.datatypes.Address methods
    jmethodID addressWrap;
    jmethodID addressToBytes;

    // org.hyperledger.besu.datatypes.Wei methods
    jmethodID weiOf;
    jmethodID weiGetValue;

    // org.apache.tuweni.units.bigints.UInt256 methods
    jmethodID uint256Of;
    jmethodID uint256ToBytes;

    // java.util.Optional methods
    jmethodID optionalOf;
    jmethodID optionalEmpty;
    jmethodID optionalIsPresent;
    jmethodID optionalGet;

    // MessageFrame methods
    jmethodID mfGetPC;
    jmethodID mfSetPC;
    jmethodID mfGetRemainingGas;
    jmethodID mfSetGasRemaining;
    jmethodID mfDecrementRemainingGas;
    jmethodID mfGetStackItem;
    jmethodID mfPopStackItem;
    jmethodID mfPushStackItem;
    jmethodID mfStackSize;
    jmethodID mfReadMemory;
    jmethodID mfWriteMemory;
    jmethodID mfExpandMemory;
    jmethodID mfGetState;
    jmethodID mfSetState;
    jmethodID mfGetCode;
    jmethodID mfGetWorldUpdater;
    jmethodID mfSetExceptionalHaltReason;
    jmethodID mfGetExceptionalHaltReason;
    jmethodID mfGetRecipientAddress;
    jmethodID mfGetSenderAddress;
    jmethodID mfGetContractAddress;

    // OperationTracer methods
    jmethodID otTracePreExecution;
    jmethodID otTracePostExecution;
    jmethodID otTraceContextEnter;
    jmethodID otTraceContextReEnter;
    jmethodID otTraceContextExit;

    // Operation.OperationResult constructor
    jmethodID operationResultInit;

    // Code methods
    jmethodID codeGetSize;
    jmethodID codeGetBytes;

    static JniCache& getInstance(JNIEnv* env);

private:
    static std::unique_ptr<JniCache> instance_;
};

/**
 * Conversion functions between Java and C++ types.
 */
namespace convert {

// Bytes conversions
Bytes jbytesToBytes(JNIEnv* env, jobject jbytes);
jobject bytesToJBytes(JNIEnv* env, const Bytes& bytes);
Bytes jbyteArrayToBytes(JNIEnv* env, jbyteArray jarray);
jbyteArray bytesToJByteArray(JNIEnv* env, const Bytes& bytes);

// Address conversions
Address jaddressToAddress(JNIEnv* env, jobject jaddress);
jobject addressToJAddress(JNIEnv* env, const Address& address);

// Wei conversions
Wei jweiToWei(JNIEnv* env, jobject jwei);
jobject weiToJWei(JNIEnv* env, const Wei& wei);

// UInt256 conversions
UInt256 juint256ToUInt256(JNIEnv* env, jobject juint256);
jobject uint256ToJUInt256(JNIEnv* env, const UInt256& value);

// Hash conversions
Hash jhashToHash(JNIEnv* env, jobject jhash);
jobject hashToJHash(JNIEnv* env, const Hash& hash);

// Optional conversions
template<typename T>
std::optional<T> joptionalToOptional(JNIEnv* env, jobject jopt,
                                     std::function<T(JNIEnv*, jobject)> converter);

template<typename T>
jobject optionalToJOptional(JNIEnv* env, const std::optional<T>& opt,
                            std::function<jobject(JNIEnv*, const T&)> converter);

// String conversions
std::string jstringToString(JNIEnv* env, jstring jstr);
jstring stringToJString(JNIEnv* env, const std::string& str);

// Primitive conversions
inline jlong int64ToJLong(int64_t value) { return static_cast<jlong>(value); }
inline int64_t jlongToInt64(jlong value) { return static_cast<int64_t>(value); }
inline jint int32ToJInt(int32_t value) { return static_cast<jint>(value); }
inline int32_t jintToInt32(jint value) { return static_cast<int32_t>(value); }

}  // namespace convert

}  // namespace jni
}  // namespace evm
}  // namespace besu

// Template implementations
namespace besu {
namespace evm {
namespace jni {
namespace convert {

template<typename T>
std::optional<T> joptionalToOptional(JNIEnv* env, jobject jopt,
                                     std::function<T(JNIEnv*, jobject)> converter) {
    if (jopt == nullptr) {
        return std::nullopt;
    }

    auto& cache = JniCache::getInstance(env);
    jboolean isPresent = env->CallBooleanMethod(jopt, cache.optionalIsPresent);
    if (!isPresent) {
        return std::nullopt;
    }

    jobject value = env->CallObjectMethod(jopt, cache.optionalGet);
    if (value == nullptr) {
        return std::nullopt;
    }

    return converter(env, value);
}

template<typename T>
jobject optionalToJOptional(JNIEnv* env, const std::optional<T>& opt,
                            std::function<jobject(JNIEnv*, const T&)> converter) {
    auto& cache = JniCache::getInstance(env);

    if (!opt.has_value()) {
        return env->CallStaticObjectMethod(cache.optionalClass, cache.optionalEmpty);
    }

    jobject jvalue = converter(env, opt.value());
    return env->CallStaticObjectMethod(cache.optionalClass, cache.optionalOf, jvalue);
}

}  // namespace convert
}  // namespace jni
}  // namespace evm
}  // namespace besu
