/*
 * validation.h defines the structure of the validation section of a block.
 *
 *
 *  **Validation Structure**
 *  The validation block has two parts.
 *  1. A summary of the change in chain state caused by the transactions.
 *  2. A map of miner addresses to signatures.
 *
 *  **Core Validation Logic**
 *  All of the following statements must be true;
 *  1. The sum of all xfer.amounts in a transaction must be 0.
 *  2. Every xfer.sig must validate against its xfer.addr.
 *  3. In 'exchange' operations, for each sender, xfer.amount must exactly equal the current sum of its onchain coins.
 *  4. In 'create','modify','delete' operations, each sender addr must be part of the INN.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef SRC_PRIMITIVES_VALIDATION_H_
#define SRC_PRIMITIVES_VALIDATION_H_

#include <cstdint>
#include <map>
#include <vector>

#include "primitives/buffers.h"
#include "SmartCoin.h"

namespace Devv {

/// A map of validations - Addresses and Signatures
typedef std::map<Address, Signature> ValidationMap;

/**
 * A validation.
 */
class Validation {
 public:
  /**
   * Constructor
   * @param other
   */
  Validation(const Validation& other) = default;

  /**
   * Constructor
   * @param validation_map
   */
  explicit Validation(const ValidationMap& validation_map) : sigs_(validation_map) {}

  /**
   * Create an empty Validation
   * @return
   */
  static Validation Create();

  /**
   * Create a Validation and populate from InputBuffer
   * @param buffer input buffer to create Validation
   * @return validation object
   */
  static Validation Create(InputBuffer& input_buffer);

  /**
   * Create a Validation from the input buffer. Include count Address/Signature
   * pairs
   * @param buffer InputBuffer to deserialize Validation from
   * @param count Number of Address/Signatures to deserialize
   * @return Validation object
   */
  static Validation Create(InputBuffer& input_buffer, uint32_t count);

  /** Adds a validation record to this block.
   *  @param node the address of node that produced this validation
   *  @param sig the signature of the validating node
   *  @return true iff the validation verified against the node's public key
   */
  bool addValidation(const Address& node, const Signature& sig) {
    if (node.size() != kNODE_ADDR_BUF_SIZE) {
      throw std::runtime_error("Address must be a node Address");
    }
    if (sig.size() != kNODE_SIG_BUF_SIZE) {
      throw std::runtime_error("Signature must be a node Signature");
    }
    sigs_.insert(std::pair<Address, Signature>(node, sig));
    return true;
  }

  /**
   * Add other to the ValidationMap
   *
   * @param other
   * @return
   */
  bool addValidation(const Validation& other) {
    sigs_.insert(other.sigs_.begin(), other.sigs_.end());
    return true;
  }

  /**
   * Return the first validation
   * @return
   */
  std::pair<Address, Signature> getFirstValidation() {
    auto x = sigs_.begin();
    if (x == sigs_.end()) {
      throw std::range_error("ValidationMap is empty");
    }
    std::pair<Address, Signature> pair(x->first, x->second);
    return pair;
  }

  /**
   * Returns a CBOR byte vector representing this validation block.
   * @return a CBOR byte vector representing this validation block.
   */
  std::vector<byte> getCanonical() const {
    MTR_SCOPE_FUNC();
    std::vector<byte> serial;
    for (auto const& item : sigs_) {
      std::vector<byte> bin_addr(item.first.getCanonical());
      serial.insert(serial.end(), bin_addr.begin(), bin_addr.end());
      std::vector<byte> bin_sig(item.second.getCanonical());
      serial.insert(serial.end(), bin_sig.begin(), bin_sig.end());
    }
    return serial;
  }

  /**
   * Returns the size of this verification block in bytes
   * @return the size of this verification block in bytes
   */
  unsigned int getByteSize() const { return getCanonical().size(); }

  /**
   * Returns the number of validations in this block.
   * @return the number of validations in this block.
   */
  unsigned int getValidationCount() const { return sigs_.size(); }

  /**
   * Returns the hash of this validation block.
   * @return the hash of this validation block.
   */
  const Hash getHash() const { return DevvHash(getCanonical()); }

  /**
   * Return the size of a pair (Address + Signature)
   * @return
   */
  static size_t pairSize() { return kNODE_ADDR_BUF_SIZE + kNODE_SIG_BUF_SIZE; }

  /**
   * Get a reference to the ValidationMap
   * @return
   */
  const ValidationMap& getValidationMap() const { return sigs_; }

 private:
  /**
   * Default constructor
   */
  Validation() = default;

  /// Map of signatures
  ValidationMap sigs_;
};

inline Validation Validation::Create() {
  return(Validation());
}

inline Validation Validation::Create(InputBuffer& buffer) {
  MTR_SCOPE_FUNC();
  Validation validation;
  size_t remainder = buffer.size() - buffer.getOffset();
  while (remainder >= pairSize()) {
    Address one_addr;
    Signature one_sig;
    buffer.copy(one_addr);
    buffer.copy(one_sig);
    std::pair<Address, Signature> one_pair(one_addr, one_sig);
    validation.sigs_.insert(one_pair);
    remainder -= pairSize();
  }
  return validation;
}

inline Validation Validation::Create(InputBuffer& buffer, uint32_t count) {
  MTR_SCOPE_FUNC();
  Validation validation;
  if (buffer.size() < buffer.getOffset() + (count * pairSize())) {
    LOG_WARNING << "Invalid Validation, too small";
  }
  size_t remainder = count;
  while (remainder > 0) {
    Address one_addr;
    buffer.copy(one_addr);
    Signature one_sig;
    buffer.copy(one_sig);
    std::pair<Address, Signature> one_pair(one_addr, one_sig);
    validation.sigs_.insert(one_pair);
    remainder--;
  }
  return validation;
}

}  // end namespace Devv

#endif /* SRC_PRIMITIVES_VALIDATION_H_ */
