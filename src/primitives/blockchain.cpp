/**
 * blockchain.h
 * Provides access to blockchain structures.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "primitives/blockchain.h"

#include <boost/filesystem.hpp>

namespace Devv {

namespace fs = boost::filesystem;

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

  std::map<std::vector<byte>, std::vector<byte>> Blockchain::TraceTransactions(
                                                 const std::vector<byte>& target) const {
    std::map<std::vector<byte>, std::vector<byte>> txs;

    size_t start_seg = getSegmentIndexAt(segment_capacity_);
    //skips any blocks that have been pruned from memory
    if (size() > 0 && start_seg >= prune_cursor_) {
      size_t start_block = 0;
      for (auto i = start_seg; i < getCurrentSegmentIndex(); i++) {
        for (auto j = start_block; j < chain_.at(i).size(); j++) {
          std::vector<byte> canonical = chain_.at(i).at(j)->getCanonical();
          InputBuffer buffer(canonical);
          ChainState state;
          FinalBlock one_block(FinalBlock::Create(buffer, state));
          for (const auto& raw_tx : one_block.getRawTransactions()) {
            if(std::search(std::begin(raw_tx), std::end(raw_tx)
                , std::begin(target), std::end(target)) != std::end(raw_tx)) {
              InputBuffer t2_buffer(raw_tx);
              Tier2Transaction t2tx = Tier2Transaction::QuickCreate(t2_buffer);
              txs.insert(std::make_pair(t2tx.getSignature().getCanonical(), raw_tx));
            }
          }
        }
        start_block = 0;
      }
    }
    return txs;
  }

 void Blockchain::Fill(const std::string& working_dir,
           const KeyRing& keys,
           eAppMode mode) {
    LOG_DEBUG << "Looking for blockchain at: " << working_dir;
    Hash prev_hash = DevvHash({'G', 'e', 'n', 'e', 's', 'i', 's'});
    fs::path p(working_dir);
    if (!p.empty() && is_directory(p)) {
      std::vector<std::string> segments;
      for (auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
        segments.push_back(entry.path().string());
      }
      std::sort(segments.begin(), segments.end());
      for (auto const& seg_path : segments) {
        fs::path seg(seg_path);
        if (!seg.empty() && is_directory(seg)) {
          std::vector<std::string> files;
          for (auto& seg_entry : boost::make_iterator_range(fs::directory_iterator(seg), {})) {
            files.push_back(seg_entry.path().string());
          }
          std::sort(files.begin(), files.end());
          for (auto const& file_name : files) {
            LOG_DEBUG << "Reading " << file_name;
            std::ifstream file(file_name, std::ios::binary);
            file.unsetf(std::ios::skipws);
            std::size_t file_size;
            file.seekg(0, std::ios::end);
            file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            std::vector<byte> raw;
            raw.reserve(file_size);
            raw.insert(raw.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
            if (IsBlockData(raw)) {
              InputBuffer buffer(raw);
              while (buffer.getOffset() < static_cast<size_t>(file_size)) {
                try {
                  ChainState prior = getHighestChainState();
                  auto new_block = std::make_shared<FinalBlock>(buffer, prior, keys, mode);
                  Hash p_hash = new_block->getPreviousHash();
                  if (!std::equal(std::begin(prev_hash), std::end(prev_hash), std::begin(p_hash))) {
                    LOG_FATAL
                      << "CHAINBREAK: The previous hash referenced in this block does not match the expected hash.";
                    break;
                  } else {
                    prev_hash = DevvHash(new_block->getCanonical());
                  push_back(new_block);
                  }
                } catch (const std::exception& e) {
                  LOG_ERROR << "Error scanning " << file_name
                            << " skipping to next file.  Error details: " + FormatException(&e, "validator.init");
                  break;
                }
              }
            } else {
              LOG_WARNING << "Working directory contained non-block binary data at: " << file_name;
            }
          }  //end file for loop
        } else {
          LOG_INFO << "Empty segment " << seg_path;
        }
      } //end segment for loop
    } else {
      LOG_INFO << "No existing blocks found, starting from Genesis.";
    }
  }

} // namespace Devv
