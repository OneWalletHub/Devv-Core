/*
 * Address.h - structure to represent public keys
 * has a 1 byte prefix indicating the key type.
 *
 * Strategy: Twin of Signature.h.
 * If you're changing Address.h, you likely need to change Signature.h too.
 * Use the prefix byte to enumerate Address types.
 * The type prefix is currently equal to the remainder length.
 *
 *  The constructor works with or without the 1 byte type prefix.
 *
 *  Allowed values in this version:
 *  0 -> Null address, nothing follows, usually an error state
 *  33 -> raw Wallet Address (secp256k1) follows, 33 bytes
 *  49 -> raw Node Address (secp384r1) follows, 49 bytes
 *
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef PRIMITIVES_ADDRESS_H_
#define PRIMITIVES_ADDRESS_H_

#include <stdint.h>
#include <algorithm>

#include "common/devv_constants.h"

namespace Devv {

class Address {
 public:
  Address() = default;
  /**
   *
   * @param vec
   */
  Address(const std::vector<byte>& vec)
      : canonical_(vec) {
    if (vec.size() == kWALLET_ADDR_SIZE) {
      //prepend type
      canonical_.insert(std::begin(canonical_), kWALLET_ADDR_SIZE);
    } else if (vec.size() == kWALLET_ADDR_BUF_SIZE
        && vec.at(0) == kWALLET_ADDR_SIZE) {
      //already good, copied from argument
    } else if (vec.size() == kNODE_ADDR_SIZE) {
      //prepend type
      canonical_.insert(std::begin(canonical_), kNODE_ADDR_SIZE);
    } else if (vec.size() == kNODE_ADDR_BUF_SIZE
        && vec.at(0) == kNODE_ADDR_SIZE) {
      //already good, copied from argument
    } else {
      LOG_ERROR << "Invalid Address size: "
            + std::to_string(vec.size()) + " : " + Bin2Str(vec);

    }
  }

  /**
   *
   * @param other
   */
  Address(const Address& other) = default; // : canonical_(other.canonical_) {}

  /** Compare addresses */
  friend bool operator==(const Address& a, const Address& b) { return (a.canonical_ == b.canonical_); }
  /** Compare addresses */
  friend bool operator!=(const Address& a, const Address& b) { return (a.canonical_ != b.canonical_); }
  friend bool operator<(const Address& a, const Address& b) { return (a.canonical_ < b.canonical_); }
  friend bool operator<=(const Address& a, const Address& b) { return (a.canonical_ <= b.canonical_); }
  friend bool operator>(const Address& a, const Address& b) { return (a.canonical_ > b.canonical_); }
  friend bool operator>=(const Address& a, const Address& b) { return (a.canonical_ >= b.canonical_); }

  /** Assign address */
  Address& operator=(const Address&& other) {
    if (this != &other) {
      this->canonical_ = other.canonical_;
    }
    return *this;
  }

  /** Assign address */
  Address& operator=(const Address& other) {
    if (this != &other) {
      this->canonical_ = other.canonical_;
    }
    return *this;
  }

  static size_t getSizeByType(byte addr_type) {
    if (addr_type == kWALLET_ADDR_SIZE) {
      return kWALLET_ADDR_SIZE;
    } else if (addr_type == kNODE_ADDR_SIZE) {
      return kNODE_ADDR_SIZE;
    } else {
      LOG_ERROR << "Invalid Address size.";
      return 0;
    }
  }

  byte at(size_t loc) const {
    return canonical_.at(loc);
  }

  /**
   * Return the size of this Address
   * @return size of this Address
   */
  size_t size() const { return canonical_.size(); }

  /**
   * Gets this address in a canonical form.
   * @return a vector defining this transaction in canonical form.
   */
  const std::vector<byte>& getCanonical() const { return canonical_; }

  /**
   * Return the JSON representation of this Address
   * @return hex string of this Address
   */
  std::string getJSON() const {
    MTR_SCOPE_FUNC();
    if (isNull()) return "";
    const char alpha[] = "0123456789ABCDEF";
    std::stringstream ss;
    for (size_t j = 0; j < canonical_.size(); j++) {
      int c = (int)canonical_.at(j);
      ss.put(alpha[(c >> 4) & 0xF]);
      ss.put(alpha[c & 0xF]);
    }
    return (ss.str());
  }

  /**
   * @return type byte for this address
   */
  byte getAddressType() {
    if (isNull()) throw std::runtime_error("Address is not initialized!");
    return canonical_.at(0);
  }

  /**
   * Check if Address is a wallet Address
   * @return true iff Address is wallet address (34 bytes)
   * @return false otherwise
   */
  bool isWalletAddress() const {
    if (isNull()) return false;
    return (canonical_.at(0) == kWALLET_ADDR_SIZE);
  }

  /**
   * Check if Address is a node Address
   * @return true iff Address is node address (49 bytes)
   * @return false otherwise
   */
  bool isNodeAddress() const {
    if (isNull()) return false;
    return (canonical_.at(0) == kNODE_ADDR_SIZE);
  }

  bool isNull() const {
    if (canonical_.empty()) return true;
    return (canonical_.at(0) == 0);
  }

  /**
   * @return this address without the type prefix.
   */
  std::vector<byte> getAddressRaw() const {
    if (isNull()) throw std::runtime_error("Address is not initialized!");
	std::vector<byte> raw(canonical_.begin()+1, canonical_.end());
    return raw;
  }

  /**
   * @return this address without the type prefix.
   */
  std::string getHexString() const {
    if (isNull()) throw std::runtime_error("Address is not initialized!");
	std::string str = getJSON();
    return str.erase(0,2);
  }

 private:
  /// The canonical representation of this Address
  std::vector<byte> canonical_{0};
};

}  // end namespace Devv

#endif /* PRIMITIVES_SIGNATURE_H_ */
