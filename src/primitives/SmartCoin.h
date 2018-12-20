/*
 * smartcoin.h defines the structure of an abstract 'Tier 1' coin
 *
 *  Created on: Jan 12, 2018
 *  Author: Nick Williams
 */

#ifndef SRC_PRIMITIVES_SMARTCOIN_H_
#define SRC_PRIMITIVES_SMARTCOIN_H_

#include <string>
#include <vector>

#include "common/ossladapter.h"
#include "common/util.h"

namespace Devv {

// keep coins within uint64, divisible to 10^-8
/// @todo (mckenney) move to constants
static const uint64_t kCOIN = 100000000;
static const uint64_t kMAX_COIN = 184000000000 * kCOIN;

/**
 * A smart coin.
 */
class SmartCoin {
 public:
  /**
   *
   * @param addr
   * @param coin
   * @param amount
   */
  SmartCoin(const Address& addr, uint64_t coin, int64_t amount = 0)
      : addr_(addr), coin_(coin), amount_(amount) {}

 public:
  /**
   *
   */
  const Address& getAddress() const {
    return addr_;
  }
  /**
   * Set the address
   * @param addr_
   */
  void setAddress(const Address& addr) {
    addr_ = addr;
  }
  /**
   *
   * @return
   */
  uint64_t getCoin() const {
    return coin_;
  }
  /**
   *
   * @param coin_
   */
  void setCoin(uint64_t coin) {
    coin_ = coin;
  }
  /**
   *
   * @return
   */
  int64_t getAmount() const {
    return amount_;
  }
  /**
   *
   * @param amount_
   */
  void setAmount(int64_t amount) {
    amount_ = amount;
  }

 private:
  /// Address
  Address addr_;
  /// Coin
  uint64_t coin_;
  /// Amount
  int64_t amount_ = 0;
};

}  // end namespace Devv

#endif /* SRC_PRIMITIVES_SMARTCOIN_H_ */
