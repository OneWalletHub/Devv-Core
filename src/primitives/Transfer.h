/*
 * Transfer.h - structure to represent the movement of
 * SmartCoins from one address to another
 *
 *  Created on: Apr 17, 2018
 *      Author: Nick Williams
 */

#ifndef PRIMITIVES_TRANSFER_H_
#define PRIMITIVES_TRANSFER_H_

#include <stdint.h>
#include <algorithm>
#include <string>

#include "SmartCoin.h"
#include "primitives/buffers.h"
#include "common/binary_converters.h"
#include "consensus/KeyRing.h"
#include "consensus/chainstate.h"

namespace Devcash {

/// @todo (mckenney) move to constants file
static const std::string kTYPE_TAG = "type";
static const std::string kDELAY_TAG = "dlay";
static const std::string kADDR_TAG = "addr";
static const std::string kAMOUNT_TAG = "amount";

/**
 * A single coin transfer
 */
class Transfer {
 public:
  /**
   *
   * @param addr
   * @param coin
   * @param amount
   * @param delay
   */
  Transfer(const Address& addr,
           uint64_t coin,
           int64_t amount,
           uint64_t delay)
      : canonical_(addr.getCanonical()) {
    Uint64ToBin(coin, canonical_);
    Int64ToBin(amount, canonical_);
    Uint64ToBin(delay, canonical_);
  }

  /**
   * Create a transfer from the buffer serial and update the offset
   * @param[in] serial
   * @param[in, out] offset
   */
  explicit Transfer(InputBuffer& buffer)
      : canonical_() {
    /// @todo - check for appropriate buffer size
    // Get address from buffer
    Address addr;
    buffer.copy(addr);
    canonical_ = addr.getCanonical();

    // Copy out coin, amount, delay
    buffer.copy(std::back_inserter(canonical_), 8*3);
  }

  /**
   *
   * @param other
   */
  Transfer(const Transfer& other) = default; // : canonical_(other.canonical_) {}

  /** Compare transfers */
  friend bool operator==(const Transfer& a, const Transfer& b) { return (a.canonical_ == b.canonical_); }
  /** Compare transfers */
  friend bool operator!=(const Transfer& a, const Transfer& b) { return (a.canonical_ != b.canonical_); }

  /** Assign transfers */
  Transfer& operator=(const Transfer&& other) {
    if (this != &other) {
      this->canonical_ = other.canonical_;
    }
    return *this;
  }

  /** Assign transfers */
  Transfer& operator=(const Transfer& other) {
    if (this != &other) {
      this->canonical_ = other.canonical_;
    }
    return *this;
  }

  /**
   * Return the size of this Transfer
   * @return size of this Transfer
   */
  size_t Size() const { return canonical_.size(); }

  static size_t MinSize() { return kWALLET_ADDR_BUF_SIZE + 24; }
  static size_t MaxSize() { return kNODE_ADDR_BUF_SIZE + 24; }

  /**
   * Gets this transfer in a canonical form.
   * @return a vector defining this transaction in canonical form.
   */
  const std::vector<byte>& getCanonical() const { return canonical_; }

  /**
   * Return the JSON representation of this Transfer
   * @return
   */
  std::string getJSON() const {
    std::string json("{\"" + kADDR_TAG + "\":\"");
    Address addr = getAddress();
    json += addr.getJSON();
    json += "\",\"" + kTYPE_TAG + "\":" + std::to_string(getCoin());
    json += ",\"" + kAMOUNT_TAG + "\":" + std::to_string(getAmount());
    json += ",\"" + kDELAY_TAG + "\":" + std::to_string(getDelay());
    json += "}";
    return json;
  }

  /**
   * Get the address of this coin
   * @return
   */
  Address getAddress() const {
	if (canonical_.at(0) == kWALLET_ADDR_SIZE) {
      std::vector<byte> addr(canonical_.begin()
                            , canonical_.begin()+kWALLET_ADDR_SIZE + 1);
      return addr;
	} else if (canonical_.at(0) == kNODE_ADDR_SIZE) {
      std::vector<byte> addr(canonical_.begin()
                            , canonical_.begin()+kNODE_ADDR_SIZE + 1);
      return addr;
	}
	std::string err = "Transfer Address has invalid type prefix.";
	throw std::runtime_error(err);
  }

  /**
   * Return the coin
   * @return
   */
  uint64_t getCoin() const {
	if (canonical_.at(0) == kWALLET_ADDR_SIZE) {
      return BinToUint64(canonical_, kWALLET_ADDR_BUF_SIZE);
	} else if (canonical_.at(0) == kNODE_ADDR_SIZE) {
      return BinToUint64(canonical_, kNODE_ADDR_BUF_SIZE);
	}
	std::string err = "Transfer has invalid type prefix.";
	throw std::runtime_error(err);
  }

  /**
   * Return the amount of this Transfer
   * @return
   */
  int64_t getAmount() const {
	if (canonical_.at(0) == kWALLET_ADDR_SIZE) {
      return BinToInt64(canonical_, kWALLET_ADDR_BUF_SIZE + 8);
	} else if (canonical_.at(0) == kNODE_ADDR_SIZE) {
      return BinToInt64(canonical_, kNODE_ADDR_BUF_SIZE + 8);
	}
	std::string err = "Transfer has invalid type prefix.";
	throw std::runtime_error(err);
  }

  /**
   * Returns the delay in seconds until this transfer is final.
   * @return the delay in seconds until this transfer is final.
   */
  uint64_t getDelay() const {
	if (canonical_.at(0) == kWALLET_ADDR_SIZE) {
      return BinToUint64(canonical_, kWALLET_ADDR_BUF_SIZE + 16);
	} else if (canonical_.at(0) == kNODE_ADDR_SIZE) {
      return BinToUint64(canonical_, kNODE_ADDR_BUF_SIZE + 16);
	}
	std::string err = "Transfer has invalid type prefix.";
	throw std::runtime_error(err);
  }

 private:
  /// The canonical representation of this Transfer
  std::vector<byte> canonical_;
};

typedef std::unique_ptr<Transfer> TransferPtr;

}  // end namespace Devcash

#endif /* PRIMITIVES_TRANSFER_H_ */
