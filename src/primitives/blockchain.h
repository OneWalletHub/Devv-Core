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
 * Holds the blockchain
 */
class Blockchain {
  /**
   * The block_index struct contains the index of the segment
   * within the chain and then location of the block within the segment.
   * It is used to index the block location within the underlying
   * blockchain representation.
   */
  struct block_index {
    block_index(size_t seg_index, size_t seg_height)
        : segment_index(seg_index)
        , segment_height(seg_height) {
    }
    /// The segment index for this block
    size_t segment_index;
    /// The block index within the segment
    size_t segment_height;
  };

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
  bool push_back(FinalBlockSharedPtr block);

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
    auto bindex = getBlockIndexFromBlockHeight(height);
    return chain_.at(bindex.segment_index).at(bindex.segment_height)->getCanonical();
  }

  /**
   * @return a pointer to a given block in this chain.
   */
  const std::vector<byte> raw_at(size_t height) const {
    LOG_TRACE << name_ << ": at(); size(" << chain_size_ << ")";
    auto bindex = getBlockIndexFromBlockHeight(height);
    return chain_.at(bindex.segment_index).at(bindex.segment_height)->getCanonical();
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
    return int(std::floor(segment/segment_capacity_));
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
    auto bindex = getBlockIndexFromBlockHeight(loc);
    return chain_.at(bindex.segment_index).at(bindex.segment_height);
  }

  /**
   * @return the highest Merkle root in this chain.
   */
  const Hash& getHighestMerkleRoot() const {
    if (chain_size_ < 1) {
      return genesis_merkle_root_;
    } else {
      return back()->getMerkleRoot();
    }
  }

  /**
   * @return the highest chain state of this chain
   */
  const ChainState& getHighestChainState() const {
    if (chain_size_ < 1) {
      LOG_DEBUG << "chain_size: " << chain_size_ << " empty_chainstate_.size(): " << empty_chainstate_.size();
      return empty_chainstate_;
    } else {
      LOG_DEBUG << "chain_size: " << chain_size_ << " chainstate.size(): " << back()->getChainState().size();
      return back()->getChainState();
    }
  }

  /**
   * Search transactions in this chain for a specific bytestring, such as a signature.
   *
   * @param target - the bytestring needle to search for in the chain
   * @return a map of signatures to transactions containing the target bytestring
   * @return if the target is not found, the returned map is empty
   */
  std::map<std::vector<byte>, std::vector<byte>> TraceTransactions(
                                                 const std::vector<byte>& target);

  /**
   * @return a binary representation of this entire chain.
   */
  std::vector<byte> dumpChainInBinary() const;

  /**
   * A binary representation of this chain from a given height to the highest block.
   * @note this interface skips old blocks that have pruned from memory
   * @param start - the block height to start with
   * @return a binary representation of this chain from start to the highest block.
   */
  std::vector<byte> dumpPartialChainInBinary(size_t start) const;

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
  /// Empty hash to return if the chain is empty
  const Hash genesis_merkle_root_ = {};
  /// Empty chainstate to return if the chain is empty
  const ChainState empty_chainstate_;

  /**
   * Calculates the segment number and segment height from block height.
   * The block height represents the overall number of blocks in this chain. The segment
   * number is the number of segments in this chain and the segment height represents
   * the number of blocks in this segment. The block_index struct it a helper
   * to hold the segment number and segment height.
   * @param height The number of blocks in this chain
   * @return the block_index representing the segment number and segment height
   */
  block_index getBlockIndexFromBlockHeight(size_t height) const;

  /**
   * Clears segments if more than 2 blocks_per_segment_ blocks are loaded.
   * @return true iff blocks were pruned
   * @return false if no blocks were pruned
   */
  bool prune();

};

typedef std::shared_ptr<Blockchain> BlockchainPtr;

} // namespace Devv
