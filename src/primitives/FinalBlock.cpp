/*
 * FinalBlock.cpp
 * Implements tools to Create final blocks
 *
 * @copywrite  2018 Devvio Inc
 */
#include "primitives/FinalBlock.h"
#include "common/devv_exceptions.h"

namespace Devv {

FinalBlock FinalBlock::Create(InputBuffer &buffer, const ChainState &prior) {
  FinalBlock block(prior);
  if (buffer.size() < MinSize()) {
    throw DeserializationError("Invalid serialized FinalBlock, too small!");
  }
  block.version_ |= buffer.getNextByte();
  if (block.version_ != 0) {
    throw DeserializationError("Invalid FinalBlock.version: " + std::to_string(block.version_));
  }
  block.num_bytes_ = buffer.getNextUint64();
  if (buffer.size() < block.num_bytes_) {
    throw DeserializationError("Invalid serialized FinalBlock, wrong size!");
  }

  block.block_time_ = buffer.getNextUint64();
  buffer.copy(block.prev_hash_);
  buffer.copy(block.merkle_root_);
  block.tx_size_ = buffer.getNextUint64();
  block.sum_size_ = buffer.getNextUint64();
  block.val_count_ = buffer.getNextUint32();

  // this constructor does not parse transactions, but groups their canonical forms
  size_t pre_tx_offset = buffer.getOffset();
  while (buffer.getOffset() < pre_tx_offset+block.tx_size_) {
    block.raw_transactions_.push_back(buffer.getNextTransaction());
  }

  block.summary_ = Summary::Create(buffer);
  block.vals_ = Validation::Create(buffer, block.val_count_);
  return block;
}

} // namespace Devv