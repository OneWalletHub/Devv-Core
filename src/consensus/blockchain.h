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
  typedef std::shared_ptr<const FinalBlock> ConstBlockSharedPtr;

  explicit Blockchain(const std::string& name)
    : name_(name), chain_size_(0), num_transactions_(0), genesis_time_(0)
    , prune_cursor_(0)
  {
  }

  ~Blockchain() = default;

  /**
   * Add a block to this chain.
   * @param block - a shared pointer to the block to add
   * @return true if a new segment was created
   * @return false if a new segment was not created
   */
  bool push_back(BlockSharedPtr block) {
    size_t seg_num = std::min(chain_size_/kBLOCKS_PER_SEGMENT);
    bool new_seg = false;
    if (chain_size_ % kBLOCKS_PER_SEGMENT == 0) {
      new_seg = true;
      std::vector<BlockSharedPtr> seg();
      seg.push_back(block);
      chain.push_back(seg);
    } else {
      chain[seg_num].push_back(block);
    }
    if (chain_size_ == 0) {
      genesis_time_ = block->getBlockTime();
    }
    chain_size_++;
    num_transactions_ += block->getNumTransactions();

    LOG_NOTICE << name_ << "- Updating Final Blockchain - (size/ntxs)" <<
               " (" << chain_size_ << "/" << num_transactions_ << ")" <<
          " this (" << ToHex(DevvHash(block->getCanonical()), 8) << ")" <<
          " prev (" << ToHex(block->getPreviousHash(), 8) << ")";
    return new_seg;
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
    return chain_.back().back();
  }

  /**
   * @return a pointer to the highest block in this chain.
   */
  const BlockSharedPtr back() const {
    LOG_TRACE << name_ << ": back() const; size(" << chain_size_ << ")";
    return chain_.back().back();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  std::vector<byte> raw_at(size_t height) {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::min(height/kBLOCKS_PER_SEGMENT);
    size_t block_num = height % kBLOCKS_PER_SEGMENT;
    lOG_TRACE << "segment = " << seg_num;
    lOG_TRACE << "relative block = " << block_num;
    return chain_.at(seg_num).at(block_num)->getCanonical();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  const std::vector<byte> raw_at(size_t height) const {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::min(height/kBLOCKS_PER_SEGMENT);
    size_t block_num = height % kBLOCKS_PER_SEGMENT;
    lOG_TRACE << "segment = " << seg_num;
    lOG_TRACE << "relative block = " << block_num;
    return chain_.at(seg_num).at(block_num)->getCanonical();
  }

  /**
   * @return the size of this chain.
   */
  size_t size() const {
    LOG_TRACE << name_ << ": size(" << chain_size_ << ")";
    return chain_size_;
  }

  /**
   * @return the number of segments in this chain
   */
  size_t seg_size() const {
    return std::max(size()/kBLOCKS_PER_SEGMENT);
  }

  size_t get_segment_height() const {
    return (size() % kBLOCKS_PER_SEGMENT);
  }

  /**
   *
   * @param loc
   * @return
   */
  BlockSharedPtr at(size_t loc) const {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::min(height/kBLOCKS_PER_SEGMENT);
    size_t block_num = height % kBLOCKS_PER_SEGMENT;
    lOG_TRACE << "segment = " << seg_num;
    lOG_TRACE << "relative block = " << block_num;
    return chain_.at(seg_num).at(block_num);
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
    for (size_t i=prune_cursor_; i < seg_size(); i++) {
      for (auto const& item : chain_.at(i)) {
        std::vector<byte> canonical = item->getCanonical();
        out.insert(out.end(), canonical.begin(), canonical.end());
      }
    }
    return out;
  }

  /**
   * A binary representation of this chain from a given height to the penultimate block.
   * @note this interface skips old blocks that have pruned from memory
   * @param start - the block height to start with
   * @return a binary representation of this chain from start to the penultimate block.
   */
  std::vector<byte> PartialBinaryDump(size_t start) const {
    std::vector<byte> out;
    size_t start_seg = std::min(start/kBLOCKS_PER_SEGMENT);
    //skips any blocks that have been pruned from memory
    if (size() > 0 && start_seg >= prune_cursor_) {
      size_t start_block = start % kBLOCKS_PER_SEGMENT;
      //this interface should not return the top/back block
      for (size_t i=start_seg; i < seg_size(); i++) {
        for (size_t j=start_block; j < i.size()-1; j++) {
          std::vector<byte> canonical = chain_.at(i).at(j)->getCanonical();
          out.insert(out.end(), canonical.begin(), canonical.end());
        }
        start_block = 0;
      }
    }
    return out;
  }

  /**
   * Clears segments if more than 2 kBLOCKS_PER_SEGMENT blocks are loaded.
   * @return true iff blocks were pruned
   * @return false if no blocks were pruned
   */
  bool prune() {
    if (prune_cursor_ < seg_size()-2) {
      for (size_t i=prune_cursor_; i < seg_size()-2; i++) {
        chain_.at(i).clear();
      }
      prune_cursor_ = seg_size()-2;
      return true;
    }
    return false;
  }

  /**
   * Return a const ref to the underlying vector of BlockSharedPtrs
   * @return const ref to std::vector<BlockSharedPtr>
   */
  const std::vector<BlockSharedPtr>& getBlockVector() const { return chain_; }

private:
  std::vector<std::vector<BlockSharedPtr>> chain_;
  const std::string name_;
  std::atomic<int> chain_size_;
  std::atomic<int> num_transactions_;
  uint64_t genesis_time_;
  std::atomic<int> prune_cursor_;
};

typedef std::shared_ptr<Blockchain> BlockchainPtr;

} // namespace Devv
