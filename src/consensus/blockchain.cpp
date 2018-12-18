/**
 * blockchain.h
 * Provides access to blockchain structures.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "consensus/blockchain.h"

namespace Devv {

bool Blockchain::push_back(FinalBlockSharedPtr block) {
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

std::vector<byte> Blockchain::dumpChainInBinary() const {
  std::vector<byte> out;
  for (size_t i = prune_cursor_; i < getCurrentSegmentIndex(); i++) {
    for (auto const& item : chain_.at(i)) {
      std::vector<byte> canonical = item->getCanonical();
      out.insert(out.end(), canonical.begin(), canonical.end());
    }
  }
  return out;
}

std::vector<byte> Blockchain::dumpPartialChainInBinary(size_t start) const {
  std::vector<byte> out;
  size_t start_seg = getSegmentIndexAt(segment_capacity_);
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

Blockchain::block_index Blockchain::getBlockIndexFromBlockHeight(size_t height) const {
  size_t seg_num = getSegmentIndexAt(height);
  size_t block_num = getSegmentHeightAt(height);
  LOG_TRACE << "segment = " << seg_num;
  LOG_TRACE << "relative block = " << block_num;
  return(block_index(seg_num, block_num));
}

bool Blockchain::prune() {
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

} // namespace Devv
