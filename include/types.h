// Copyright ConsenSys AG.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace besu {
namespace evm {

// Forward declarations
class UInt256;

/**
 * Type alias for arbitrary byte sequences.
 * Corresponds to org.apache.tuweni.bytes.Bytes in Java.
 */
using Bytes = std::vector<uint8_t>;

/**
 * Type alias for 32-byte fixed-size byte sequences.
 * Corresponds to org.apache.tuweni.bytes.Bytes32 in Java.
 */
using Bytes32 = std::array<uint8_t, 32>;

/**
 * Ethereum address (20 bytes).
 * Corresponds to org.hyperledger.besu.datatypes.Address in Java.
 */
class Address {
public:
    static constexpr size_t SIZE = 20;
    using DataType = std::array<uint8_t, SIZE>;

    Address() : data_{} {}
    explicit Address(const DataType& data) : data_(data) {}
    explicit Address(const Bytes& bytes);

    const DataType& data() const { return data_; }
    Bytes toBytes() const;

    bool operator==(const Address& other) const { return data_ == other.data_; }
    bool operator!=(const Address& other) const { return !(*this == other); }
    bool operator<(const Address& other) const { return data_ < other.data_; }

    std::string toHexString() const;
    static Address fromHexString(const std::string& hex);

private:
    DataType data_;
};

/**
 * 32-byte hash value.
 * Corresponds to org.hyperledger.besu.datatypes.Hash in Java.
 */
class Hash {
public:
    static constexpr size_t SIZE = 32;
    using DataType = Bytes32;

    Hash() : data_{} {}
    explicit Hash(const DataType& data) : data_(data) {}
    explicit Hash(const Bytes& bytes);

    const DataType& data() const { return data_; }
    Bytes toBytes() const;

    bool operator==(const Hash& other) const { return data_ == other.data_; }
    bool operator!=(const Hash& other) const { return !(*this == other); }

    std::string toHexString() const;
    static Hash fromHexString(const std::string& hex);

private:
    DataType data_;
};

/**
 * Wei value (256-bit unsigned integer representing value in wei).
 * Corresponds to org.hyperledger.besu.datatypes.Wei in Java.
 */
class Wei {
public:
    Wei();
    explicit Wei(uint64_t value);
    explicit Wei(const UInt256& value);
    explicit Wei(const Bytes& bytes);

    const UInt256& value() const;
    Bytes toBytes() const;

    bool isZero() const;

    Wei operator+(const Wei& other) const;
    Wei operator-(const Wei& other) const;
    Wei operator*(const Wei& other) const;
    Wei operator/(const Wei& other) const;

    bool operator==(const Wei& other) const;
    bool operator!=(const Wei& other) const;
    bool operator<(const Wei& other) const;
    bool operator>(const Wei& other) const;
    bool operator<=(const Wei& other) const;
    bool operator>=(const Wei& other) const;

    std::string toString() const;

private:
    std::unique_ptr<UInt256> value_;
};

/**
 * 256-bit unsigned integer.
 * Corresponds to org.apache.tuweni.units.bigints.UInt256 in Java.
 *
 * This is a simplified interface. For production, consider using:
 * - boost::multiprecision::uint256_t
 * - intx library
 * - Custom optimized implementation
 */
class UInt256 {
public:
    UInt256();
    explicit UInt256(uint64_t value);
    explicit UInt256(const Bytes& bytes);
    UInt256(const UInt256& other);
    UInt256(UInt256&& other) noexcept;
    ~UInt256();

    UInt256& operator=(const UInt256& other);
    UInt256& operator=(UInt256&& other) noexcept;

    // Arithmetic operations
    UInt256 operator+(const UInt256& other) const;
    UInt256 operator-(const UInt256& other) const;
    UInt256 operator*(const UInt256& other) const;
    UInt256 operator/(const UInt256& other) const;
    UInt256 operator%(const UInt256& other) const;

    // Bitwise operations
    UInt256 operator&(const UInt256& other) const;
    UInt256 operator|(const UInt256& other) const;
    UInt256 operator^(const UInt256& other) const;
    UInt256 operator~() const;
    UInt256 operator<<(unsigned int shift) const;
    UInt256 operator>>(unsigned int shift) const;

    // Comparison operations
    bool operator==(const UInt256& other) const;
    bool operator!=(const UInt256& other) const;
    bool operator<(const UInt256& other) const;
    bool operator>(const UInt256& other) const;
    bool operator<=(const UInt256& other) const;
    bool operator>=(const UInt256& other) const;

    // Utility methods
    bool isZero() const;
    uint64_t toUint64() const;
    Bytes toBytes() const;
    Bytes32 toBytes32() const;
    std::string toString() const;
    std::string toHexString() const;

    static UInt256 fromBytes(const Bytes& bytes);
    static UInt256 fromBytes32(const Bytes32& bytes);
    static UInt256 fromHexString(const std::string& hex);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/**
 * Versioned hash for EIP-4844.
 * Corresponds to org.hyperledger.besu.datatypes.VersionedHash in Java.
 */
class VersionedHash {
public:
    VersionedHash() : data_{} {}
    explicit VersionedHash(const Bytes32& data) : data_(data) {}

    const Bytes32& data() const { return data_; }

    bool operator==(const VersionedHash& other) const { return data_ == other.data_; }
    bool operator!=(const VersionedHash& other) const { return !(*this == other); }

private:
    Bytes32 data_;
};

// Utility functions for hex conversion
std::string bytesToHex(const Bytes& bytes);
Bytes hexToBytes(const std::string& hex);

}  // namespace evm
}  // namespace besu
