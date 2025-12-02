// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <jni.h>
#include <memory>

namespace besu {
namespace evm {

// Forward declarations
class IMessageFrame;
struct OperationResult;

/**
 * Interface for operation tracing.
 * Corresponds to org.hyperledger.besu.evm.tracing.OperationTracer in Java.
 */
class IOperationTracer {
public:
    virtual ~IOperationTracer() = default;

    /**
     * Trace before operation execution.
     * @param frame The message frame
     */
    virtual void tracePreExecution(const IMessageFrame& frame) = 0;

    /**
     * Trace after operation execution.
     * @param frame The message frame
     * @param result The operation result
     */
    virtual void tracePostExecution(const IMessageFrame& frame, const OperationResult& result) = 0;

    /**
     * Trace entering a new context.
     * @param frame The message frame
     */
    virtual void traceContextEnter(const IMessageFrame& frame) = 0;

    /**
     * Trace re-entering a context from child.
     * @param frame The message frame
     */
    virtual void traceContextReEnter(const IMessageFrame& frame) = 0;

    /**
     * Trace exiting a context.
     * @param frame The message frame
     */
    virtual void traceContextExit(const IMessageFrame& frame) = 0;

    /**
     * Check if this is a no-op tracer (equivalent to NO_TRACING).
     * @return true if no tracing should be performed
     */
    virtual bool isNoTracing() const = 0;
};

/**
 * No-op operation tracer implementation.
 * Equivalent to OperationTracer.NO_TRACING in Java.
 */
class NoOpOperationTracer : public IOperationTracer {
public:
    void tracePreExecution(const IMessageFrame& frame) override {}
    void tracePostExecution(const IMessageFrame& frame, const OperationResult& result) override {}
    void traceContextEnter(const IMessageFrame& frame) override {}
    void traceContextReEnter(const IMessageFrame& frame) override {}
    void traceContextExit(const IMessageFrame& frame) override {}

    bool isNoTracing() const override { return true; }

    static NoOpOperationTracer& getInstance() {
        static NoOpOperationTracer instance;
        return instance;
    }

private:
    NoOpOperationTracer() = default;
};

/**
 * Operation tracer that bridges to Java implementation via JNI.
 * This class wraps a Java OperationTracer object and calls back to Java
 * methods when tracing operations.
 */
class OperationTracerJNI : public IOperationTracer {
public:
    /**
     * Constructor.
     * @param env JNI environment
     * @param jtracer Java OperationTracer object (can be null for NO_TRACING)
     */
    OperationTracerJNI(JNIEnv* env, jobject jtracer);

    /**
     * Destructor - releases global reference to Java tracer.
     */
    ~OperationTracerJNI() override;

    // Disable copy
    OperationTracerJNI(const OperationTracerJNI&) = delete;
    OperationTracerJNI& operator=(const OperationTracerJNI&) = delete;

    // IOperationTracer implementation
    void tracePreExecution(const IMessageFrame& frame) override;
    void tracePostExecution(const IMessageFrame& frame, const OperationResult& result) override;
    void traceContextEnter(const IMessageFrame& frame) override;
    void traceContextReEnter(const IMessageFrame& frame) override;
    void traceContextExit(const IMessageFrame& frame) override;

    bool isNoTracing() const override { return jtracer_ == nullptr; }

    /**
     * Get the Java tracer object.
     * @return Java tracer object (can be null)
     */
    jobject getJavaTracer() const { return jtracer_; }

private:
    JNIEnv* env_;
    jobject jtracer_;  // Global reference to Java OperationTracer (or null)
};

}  // namespace evm
}  // namespace besu
