/*
 * Summary.h
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef PRIMITIVES_SUMMARY_H_
#define PRIMITIVES_SUMMARY_H_

#include <stdint.h>
#include <map>
#include <mutex>
#include <vector>

#include "primitives/buffers.h"
#include "Transfer.h"

namespace Devv {

/**
 * Holds the change and delay of a coin
 */
struct DelayedItem {
  /**
   * Constructor
   * @param delay
   * @param delta
   */
  explicit DelayedItem(uint64_t delay_val = 0, int64_t delta_val = 0)
      : delay(delay_val), delta(delta_val) {}

  /// The delay of this item
  uint64_t delay = 0;
  /// The coin/transfer price (change)
  uint64_t delta = 0;
};

static const std::string kADDR_SIZE_TAG = "addr_size";
static const std::string kDELAY_SIZE_TAG = "delay_size";
static const std::string kCOIN_SIZE_TAG = "coin_size";

typedef std::map<uint64_t, DelayedItem> DelayedMap;
typedef std::map<uint64_t, int64_t> CoinMap;
typedef std::pair<DelayedMap, CoinMap> SummaryPair;
typedef std::map<Address, SummaryPair> SummaryMap;

/**
 * Add a coin to the existing map
 * @param[in] coin The coin to add to
 * @param[in] item
 * @param[in,out] existing
 * @return
 */
bool AddToDelayedMap(uint64_t coin, const DelayedItem &item, DelayedMap &existing);

/**
 * Add a coin to the existing map
 * @param[in] coin The coin to add to
 * @param[in] item
 * @param[in,out] existing
 * @return
 */
inline bool AddToCoinMap(uint64_t coin, const DelayedItem& item, CoinMap& existing) {
  if (existing.count(coin) > 0) {
    uint64_t the_item = existing.at(coin);
    the_item += item.delta;
    existing.at(coin) = the_item;
  } else {
    std::pair<uint64_t, int64_t> one_item(coin, item.delta);
    existing.insert(one_item);
  }
  return true;
}

/**
 * Interface to a summary map of addresses and coins.
 */
class Summary {
 public:
  /**
   * Move constructor
   * @param other
   */
  Summary(Summary&& other) noexcept = default;

  /**
   * Default move-assignment operator
   * @param other
   * @return
   */
  Summary& operator=(Summary&& other) = default;

  /**
   * Create a Summary from an input data stream
   * @param serial
   * @param offset
   * @return
   */
  static Summary Create(InputBuffer& buffer);

  /**
   * Create an empty Summary
   * @return
   */
  static Summary Create();

  /**
   * Perform a deep copy
   * @param summary
   * @return
   */
  static Summary Copy(const Summary& summary);

  /**
   * Minimum size of Summary
   * @return Minimum size in bytes (4)
   */
  static size_t MinSize() { return 4; }

  /**
   * Adds a summary record to this block.
   * @param[in] addr the address that this change applies to
   * @param[in] coin the coin type that this change applies to
   * @param[in] item a chain state vector summary of transactions.
   * @return true iff the summary was added
   */
  bool addItem(const Address &addr, uint64_t coin, const DelayedItem &item) {
    // std::lock_guard<std::mutex> lock(lock_);
    if (summary_.count(addr) > 0) {
      SummaryPair existing(summary_.at(addr));
      if (item.delay > 0) {
        DelayedMap delayed(existing.first);
        if (!AddToDelayedMap(coin, item, delayed)) { return false; }
        SummaryPair updated(delayed, existing.second);
        summary_.at(addr) = updated;
      } else {
        CoinMap the_map(existing.second);
        if (!AddToCoinMap(coin, item, the_map)) { return false; }
        SummaryPair updated(existing.first, the_map);
        summary_.at(addr) = updated;
      }
    } else {
      DelayedMap new_delayed;
      CoinMap new_map;
      if (item.delay > 0) {
        std::pair<uint64_t, DelayedItem> new_pair(coin, item);
        new_delayed.insert(new_pair);
      } else {
        std::pair<uint64_t, int64_t> new_pair(coin, item.delta);
        new_map.insert(new_pair);
      }
      SummaryPair new_summary(new_delayed, new_map);
      summary_.insert(std::pair<Address, SummaryPair>(addr, new_summary));
    }
    return true;
  }

  /**
   * Adds a summary record to this block.
   * @param[in] addr the addresses involved in this change
   * @param[in] coin_type the type number for this coin
   * @param[in] delta change in coins (>0 for receiving, <0 for spending)
   * @param[in] delay the delay in seconds before this transaction can be received
   * @return true iff the summary was added
   */
  bool addItem(const Address& addr, uint64_t coin_type, int64_t delta, uint64_t delay = 0) {
    DelayedItem item(delay, delta);
    return addItem(addr, coin_type, item);
  }

  /**
   * Adds a transfer
   * @param[in] transfer the transfer to add
   * @return true iff the transfer was added
   */
  bool addTransfer(const Transfer& transfer) {
    DelayedItem item(transfer.getDelay(), transfer.getAmount());
    return addItem(transfer.getAddress(), transfer.getCoin(), item);
  }

  /**
   * Get the canonical form of this summary.
   * @return a canonical bytestring summarizing these changes.
   */
  std::vector<byte> getCanonical() const {
    /// @todo (mckenney) optimization
    std::vector<byte> out;
    auto addr_count = static_cast<uint32_t>(summary_.size());
    Uint32ToBin(addr_count, out);
    for (auto summary : summary_) {
      std::vector<byte> bin_addr(summary.first.getCanonical());
      out.insert(out.end(), bin_addr.begin(), bin_addr.end());

      SummaryPair top_pair(summary.second);
      DelayedMap delayed(top_pair.first);
      CoinMap coin_map(top_pair.second);
      uint64_t delayed_count = delayed.size();
      Uint64ToBin(delayed_count, out);
      uint64_t coin_count = coin_map.size();
      Uint64ToBin(coin_count, out);
      for (auto delayed_item : delayed) {
        Uint64ToBin(delayed_item.first, out);
        Uint64ToBin(delayed_item.second.delay, out);
        Int64ToBin(delayed_item.second.delta, out);
      }
      for (auto coin : coin_map) {
        Uint64ToBin(coin.first, out);
        Int64ToBin(coin.second, out);
      }
    }
    return out;
  }

  /**
   * Get the size of the canonical bytestring
   * @return the size of the canonical bytestring
   */
  size_t getByteSize() const { return getCanonical().size(); }

  /**
   * Return a const reference to the SummaryMap
   * @return
   */
  const SummaryMap& getSummaryMap() const {
    return summary_;
  }

  /**
   * Get the transfers
   * @return a vector of Transfers
   */
  std::vector<TransferPtr> getTransfers() {
    std::vector<TransferPtr> out;
    for (auto summary : summary_) {
      SummaryPair top_pair(summary.second);
      DelayedMap delayed(top_pair.first);
      CoinMap coin_map(top_pair.second);
      for (auto delayed_item : delayed) {
        TransferPtr xfer = std::make_unique<Transfer>(summary.first
          , delayed_item.first, delayed_item.second.delta, delayed_item.second.delay);
        out.push_back(std::move(xfer));
      }
      for (auto coin : coin_map) {
        TransferPtr xfer = std::make_unique<Transfer>(summary.first
          , coin.first, coin.second, 0);
        out.push_back(std::move(xfer));
      }
    }
    return out;
  }

  /**
   * Return the number of transfers in this summary
   * @return number of transfers (coins and delayed coins)
   */
  size_t getTransferCount() const {
    size_t xfer_count = 0;
    for (auto summary : summary_) {
      SummaryPair top_pair(summary.second);
      DelayedMap delayed(top_pair.first);
      CoinMap coin_map(top_pair.second);
      xfer_count += delayed.size();
      xfer_count += coin_map.size();
    }
    return xfer_count;
  }

  /**
   * Get vector of coins corresponding to this address
   * @param[in] addr Address mapped to coins
   * @param[in] elapsed Include coins if delay < elapsed time in milliseconds
   * @return
   */
  std::vector<SmartCoin> getCoinsByAddr(const Address& addr, uint64_t elapsed) {
    std::vector<SmartCoin> out;
    if (summary_.count(addr) > 0) {
      SummaryPair existing(summary_.at(addr));
      DelayedMap delayed(existing.first);
      CoinMap coin_map(existing.second);
      for (auto delayed_item : delayed) {
        if (delayed_item.second.delay < elapsed) {
          SmartCoin coin(addr, delayed_item.first, delayed_item.second.delta);
          out.push_back(coin);
        }
      }
      for (auto this_coin : coin_map) {
        SmartCoin coin(addr, this_coin.first, this_coin.second);
        out.push_back(coin);
      }
    }
    return out;
  }

  /**
   * Perform sanity check and ensure summary sums to zero
   * @return true iff, the summary passes sanity checks
   */
  bool isSane() const;

 private:
  /**
   * Constructor
   */
  Summary() noexcept = default;

  /**
   * Copy constructor
   * @param other
   */
  Summary(const Summary& other) = default;

  /// map of summary addresses to summary pairs
  SummaryMap summary_;
};

inline Summary Summary::Create() {
  Summary new_summary;
  return new_summary;
}

inline Summary Summary::Create(InputBuffer& buffer) {
  Summary new_summary;

  if (buffer.size() < Summary::MinSize() + buffer.getOffset()) {
    std::string warning = "Invalid serialized Summary, too small!";
    LOG_WARNING << warning;
    throw std::runtime_error(warning);
  }
  size_t addr_count = buffer.getNextUint32();
  for (size_t i = 0; i < addr_count; ++i) {
    Address one_addr;
    buffer.copy(one_addr);
    DelayedMap delayed;
    CoinMap coin_map;
    size_t delayed_count = buffer.getNextUint64();
    size_t coin_count = buffer.getNextUint64();
    for (size_t j = 0; j < delayed_count; ++j) {
      uint64_t coin = buffer.getNextUint64();
      uint64_t delay = buffer.getNextUint64();
      uint64_t delta = buffer.getNextUint64();
      DelayedItem delayed_item(delay, delta);
      std::pair<uint64_t, DelayedItem> new_pair(coin, delayed_item);
      delayed.insert(new_pair);
    }
    for (size_t j = 0; j < coin_count; ++j) {
      uint64_t coin = buffer.getNextUint64();
      uint64_t delta = buffer.getNextUint64();
      std::pair<uint64_t, uint64_t> new_pair(coin, delta);
      coin_map.insert(new_pair);
    }
    SummaryPair top_pair(delayed, coin_map);
    new_summary.summary_.insert(std::pair<Address, SummaryPair>(one_addr, top_pair));
  }
  return new_summary;
}

inline Summary Summary::Copy(const Summary& summary) {
  Summary new_summary(summary);
  return new_summary;
}

}  // end namespace Devv

#endif /* PRIMITIVES_SUMMARY_H_ */
