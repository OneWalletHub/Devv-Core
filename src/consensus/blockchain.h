/**
 * blockchain.h
 * Provides access to blockchain structures.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <atomic>
#include <vector>
#include <boost/filesystem/path.hpp>

#include "common/devv_constants.h"

#include "primitives/FinalBlock.h"

namespace Devv {

/**
 * The block_index struct contains the index of the segment
 * within the chain and then location of the block within the segment.
 * It is used to index the block location within the underlying
 * blockchain representation.
 */
struct block_index {
  /// The segment index for this block
  size_t segment;
  /// The block index within the segment
  size_t block;
};

/**
 * Holds the blockchain
 */
class Blockchain {
public:
  /**
   * Constructor
   * @param name
   */
  explicit Blockchain(const std::string& name,
                      size_t segment_capacity = kDEFAULT_BLOCKS_PER_SEGMENT)
      : name_(name)
      , segment_capacity_(segment_capacity)
  {
  }

  /**
   * Default destructor
   */
  ~Blockchain() = default;

  /**
   * Add a block to this chain.
   * @param block - a shared pointer to the block to add
   * @return true if a new segment was created
   * @return false if a new segment was not created
   */
  bool push_back(FinalBlockSharedPtr block) {
    size_t seg_num = std::floor(chain_size_/segment_capacity_);
    bool new_seg = false;
    if (chain_size_ % segment_capacity_ == 0) {
      new_seg = true;
      std::vector<FinalBlockSharedPtr> seg;
      seg.reserve(segment_capacity_);
      seg.push_back(block);
      chain_.push_back(seg);
    } else {
      chain_[seg_num].push_back(block);
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

    if (new_seg) prune();
    return new_seg;
  }

  /**
   * Get the number of transactions in this chain.
   * @return the number of transactions in this chain.
   */
  size_t getNumTransactions() const {
    return num_transactions_;
  }

  /**
   * @return the average time between blocks in this chain.
   */
  uint64_t getAvgBlocktime() const {
    if (chain_size_ > 1) {
      return ((back()->getBlockTime() - genesis_time_)/chain_size_);
    } else {
      return 0;
	}
  }

  /**
   * @return a pointer to the highest block in this chain.
   */
  FinalBlockSharedPtr back() {
    LOG_TRACE << name_ << ": back(); size(" << chain_size_ << ")";
    return chain_.back().back();
  }

  /**
   * @return a pointer to the highest block in this chain.
   */
  const FinalBlockSharedPtr back() const {
    LOG_TRACE << name_ << ": back() const; size(" << chain_size_ << ")";
    return chain_.back().back();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  std::vector<byte> raw_at(size_t height) {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::floor(height/segment_capacity_);
    size_t block_num = height % segment_capacity_;
    LOG_TRACE << "segment = " << seg_num;
    LOG_TRACE << "relative block = " << block_num;
    return chain_.at(seg_num).at(block_num)->getCanonical();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  const std::vector<byte> raw_at(size_t height) const {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::floor(height/segment_capacity_);
    size_t block_num = height % segment_capacity_;
    LOG_TRACE << "segment = " << seg_num;
    LOG_TRACE << "relative block = " << block_num;
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
  size_t getCurrentSegmentIndex() const {
    return getSegmentIndexAt(size());
  }

  /**
   * @return the number of segments in this chain
   */
  size_t getSegmentIndexAt(size_t segment) const {
    return std::floor(segment/segment_capacity_);
  }

  /**
   * @return the height of this chain within the active segment.
   */
  size_t getCurrentSegmentHeight() const {
    return getSegmentHeightAt(size());
  }

  /**
   * @return the height of this chain within the active segment.
   */
  size_t getSegmentHeightAt(size_t block_number) const {
    return (block_number % segment_capacity_);
  }

  /**
   *
   * @param loc - the height of the block pointer to get
   * @return a shared pointer for the block at the specified height
   */
  FinalBlockSharedPtr at(size_t loc) const {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    size_t seg_num = std::floor(loc/segment_capacity_);
    size_t block_num = loc % segment_capacity_;
    LOG_TRACE << "segment = " << seg_num;
    LOG_TRACE << "relative block = " << block_num;
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
  std::vector<byte> dumpChainInBinary() const {
    std::vector<byte> out;
    for (size_t i = prune_cursor_; i < getCurrentSegmentIndex(); i++) {
      for (auto const& item : chain_.at(i)) {
        std::vector<byte> canonical = item->getCanonical();
        out.insert(out.end(), canonical.begin(), canonical.end());
      }
    }
    return out;
  }

  /**
   * A binary representation of this chain from a given height to the highest block.
   * @note this interface skips old blocks that have pruned from memory
   * @param start - the block height to start with
   * @return a binary representation of this chain from start to the highest block.
   */
  std::vector<byte> dumpPartialChainInBinary(size_t start) const {
    std::vector<byte> out;
    size_t start_seg = std::floor(start/segment_capacity_);
    //skips any blocks that have been pruned from memory
    if (size() > 0 && start_seg >= prune_cursor_) {
      size_t start_block = start % segment_capacity_;
      for (auto i = start_seg; i < getCurrentSegmentIndex(); i++) {
        for (auto j = start_block; j < chain_.at(i).size(); j++) {
          std::vector<byte> canonical = chain_.at(i).at(j)->getCanonical();
          out.insert(out.end(), canonical.begin(), canonical.end());
        }
        start_block = 0;
      }
    }
    return out;
  }

  /**
   * Return a const ref to the underlying vector of BlockSharedPtrs
   * @return const ref to std::vector<BlockSharedPtr>
   */
  const std::vector<std::vector<FinalBlockSharedPtr>>& getBlockVector() const {
    return chain_;
  }

  const std::string& getName() const {
    return name_;
  }

 private:
  /// The blockchain. The chain is comprised of a vector of segments
  /// Each segment contains a vector of FinalBlocks
  std::vector<std::vector<FinalBlockSharedPtr>> chain_;
  /// The name of this chain
  const std::string name_;
  /// The size of this chain
  std::atomic<uint32_t> chain_size_ = ATOMIC_VAR_INIT(0);
  /// The number of transactions
  std::atomic<uint64_t> num_transactions_ = ATOMIC_VAR_INIT(0);
  /// The genesis time of this chain
  uint64_t genesis_time_ = 0;
  /// The index of the blockchain position that this chain has been
  /// pruned to
  std::atomic<uint32_t> prune_cursor_ = ATOMIC_VAR_INIT(0);
  /// How many blocks are in each segment
  const size_t segment_capacity_ = kDEFAULT_BLOCKS_PER_SEGMENT;

  /**
   * Clears segments if more than 2 blocks_per_segment_ blocks are loaded.
   * @return true iff blocks were pruned
   * @return false if no blocks were pruned
   */
  bool prune() {
    // prevent unsigned rollover error
    if (getCurrentSegmentIndex() < 2) return false;
    if (prune_cursor_ < getCurrentSegmentIndex() - 2) {
      for (size_t i = prune_cursor_; i < (getCurrentSegmentIndex() - 2); i++) {
        chain_.at(i).clear();
      }
      prune_cursor_ = getCurrentSegmentIndex() - 2;
      return true;
    }
    return false;
  }

};

typedef std::shared_ptr<Blockchain> BlockchainPtr;

} // namespace Devv
