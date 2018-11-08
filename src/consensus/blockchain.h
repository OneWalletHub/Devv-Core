/**
 * blockchain.h
 * Provides access to blockchain structures.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <atomic>
#include <vector>

#include "primitives/FinalBlock.h"

namespace Devv {

class Blockchain {
public:
  typedef std::shared_ptr<FinalBlock> BlockSharedPtr;

  explicit Blockchain(const std::string& name)
    : name_(name), chain_size_(0), num_transactions_(0), genesis_time_(0)
  {
  }

  ~Blockchain() = default;

  /**
   * Add a block to this chain.
   * @param block - a shared pointer to the block to add
   */
  void push_back(BlockSharedPtr block) {
    chain_.push_back(block);
    if (chain_size_ == 0) {
      genesis_time_ = block->getBlockTime();
    }
    chain_size_++;
    num_transactions_ += block->getNumTransactions();

    LOG_NOTICE << name_ << "- Updating Final Blockchain - (size/ntxs)" <<
               " (" << chain_size_ << "/" << num_transactions_ << ")" <<
          " this (" << ToHex(DevvHash(block->getCanonical()), 8) << ")" <<
          " prev (" << ToHex(block->getPreviousHash(), 8) << ")";
  }

  /**
   * Get the number of transactions in this chain.
   * @return the number of transactions in this chain.
   */
  size_t getNumTransactions() const {
    return num_transactions_;
  }

  uint64_t getAvgBlocktime() const {
    if (chain_size_ > 1) {
      return ((chain_.back()->getBlockTime() - genesis_time_)/chain_size_);
    } else {
      return 0;
	}
  }

  /**
   * @return a pointer to the highest block in this chain.
   */
  BlockSharedPtr back() {
    LOG_TRACE << name_ << ": back(); size(" << chain_size_ << ")";
    return chain_.back();
  }

  /**
   * @return a pointer to the highest block in this chain.
   */
  const BlockSharedPtr back() const {
    LOG_TRACE << name_ << ": back() const; size(" << chain_size_ << ")";
    return chain_.back();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  std::vector<byte> raw_at(size_t height) {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    return chain_.at(height)->getCanonical();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  const std::vector<byte> raw_at(size_t height) const {
    LOG_TRACE << name_ << ": at() const; size(" << chain_size_ << ")";
    return chain_.at(height)->getCanonical();
  }

  /**
   * @return the size of this chain.
   */
  size_t size() const {
    LOG_TRACE << name_ << ": size(" << chain_size_ << ")";
    return chain_size_;
  }

  /**
   * @return the highest Merkle root in this chain.
   */
  Hash getHighestMerkleRoot() const {
    if (chain_size_ < 1) {
      Hash genesis;
      return genesis;
    }
    return back()->getMerkleRoot();
  }

  /**
   * @return the highest chain state of this chain
   */
  ChainState getHighestChainState() const {
    LOG_DEBUG << " chain_size: " << chain_size_;
    if (chain_size_ < 1) {
      ChainState state;
      return state;
    }
    LOG_DEBUG << "back()->getChainState().size(): " << back()->getChainState().size();
    return back()->getChainState();
  }

  /**
   * @return a binary representation of this entire chain.
   */
  std::vector<byte> BinaryDump() const {
    std::vector<byte> out;
    for (auto const& item : chain_) {
      std::vector<byte> canonical = item->getCanonical();
      out.insert(out.end(), canonical.begin(), canonical.end());
    }
    return out;
  }

  /**
   * A binary representation of this chain from a given height to the penultimate block.
   * @param start - the block height to start with
   * @return a binary representation of this chain from start to the penultimate block.
   */
  std::vector<byte> PartialBinaryDump(size_t start) const {
    std::vector<byte> out;
    if (size() > 0) {
      //this interface should not return the top/back block
      for (size_t i=start; i < size()-1; i++) {
        std::vector<byte> canonical = chain_.at(i)->getCanonical();
        out.insert(out.end(), canonical.begin(), canonical.end());
      }
    }
    return out;
  }

  /**
   * Return a const ref to the underlying vector of BlockSharedPtrs
   * @return const ref to std::vector<BlockSharedPtr>
   */
  const std::vector<BlockSharedPtr>& getBlockVector() const { return chain_; }

private:
  std::vector<BlockSharedPtr> chain_;
  const std::string name_;
  std::atomic<int> chain_size_;
  std::atomic<int> num_transactions_;
  uint64_t genesis_time_;
};

} // namespace Devv
