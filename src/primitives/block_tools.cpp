/*
 * block_tools.cpp implements tools for
 * utilizing blocks
 *
 * @copywrite  2018 Devvio Inc
 */
#include "block_tools.h"

#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

namespace Devv {

namespace fs = boost::filesystem;

bool IsBlockData(const std::vector<byte>& raw) {
  //check if big enough
  if (raw.size() < FinalBlock::MinSize()) { return false; }
  //check version
  if (raw[0] != 0x00) { return false; }
  size_t offset = 9;
  uint64_t block_time = BinToUint64(raw, offset);
  // check blocktime is from 2018 or newer.
  if (block_time < 1514764800) { return false; }
  // check blocktime is in past
  if (block_time > GetMillisecondsSinceEpoch()) { return false; }
  return true;
}

bool IsTxData(const std::vector<byte>& raw) {
  // check if big enough
  if (raw.size() < Transaction::MinSize()) {
    LOG_WARNING << "raw.size()(" << raw.size() << ") < Transaction::MinSize()(" << Transaction::MinSize() << ")";
    return false;
  }
  // check transfer count
  uint64_t xfer_size = BinToUint64(raw, 0);
  size_t tx_size = Transaction::MinSize() + xfer_size;
  if (raw.size() < tx_size) {
    LOG_WARNING << "raw.size()(" << raw.size() << ") < tx_size(" << tx_size << ")";
    return false;
  }
  // check operation
  if (raw[16] >= 4) {
    LOG_WARNING << "raw[8](" << int(raw[16]) << ") >= 4";
    return false;
  }
  return true;
}

bool CompareChainStateMaps(const std::map<Address, std::map<uint64_t, int64_t>>& first,
                           const std::map<Address, std::map<uint64_t, int64_t>>& second) {
  if (first.size() != second.size()) { return false; }
  for (auto i = first.begin(), j = second.begin(); i != first.end(); ++i, ++j) {
    if (i->first != j->first) { return false; }
    if (i->second.size() != j->second.size()) { return false; }
    for (auto x = i->second.begin(), y = j->second.begin(); x != i->second.end(); ++x, ++y) {
      if (x->first != y->first) { return false; }
      if (x->second != y->second) { return false; }
    }
  }
  return true;
}

std::string WriteChainStateMap(const std::map<Address, std::map<uint64_t, int64_t>>& map) {
  std::string out("{");
  bool first_addr = true;
  for (auto e : map) {
    if (first_addr) {
      first_addr = false;
    } else {
      out += ",";
    }
    Address a = e.first;
    out += "\"" + a.getJSON() + "\":[";
    bool is_first = true;
    for (auto f : e.second) {
      if (is_first) {
        is_first = false;
      } else {
        out += ",";
      }
      out += std::to_string(f.first) + ":" + std::to_string(f.second);
    }
    out += "]";
  }
  out += "}";
  return out;
}

Tier1TransactionPtr CreateTier1Transaction(const FinalBlock& block, const KeyRing& keys) {
  const Summary &sum = block.getSummary();
  Validation val(block.getValidation());
  std::pair<Address, Signature> pair(val.getFirstValidation());
  auto tx = std::make_unique<Tier1Transaction>(sum, pair.second, pair.first, keys);
  return tx;
}

Signature SignSummary(const Summary& summary, const KeyRing& keys) {
  auto node_sig = SignBinary(keys.getNodeKey(0),
                             DevvHash(summary.getCanonical()));
  return(node_sig);
}

boost::filesystem::path GetStandardBlockPath(const Blockchain& chain,
                                             const std::string& shard_name,
                                             const boost::filesystem::path& working_dir,
                                             size_t block_index) {
  // Set the segment number
  std::stringstream ss;
  ss << std::setw(kMAX_LEFTPADDED_ZER0S) << std::setfill('0') << chain.getSegmentIndexAt(block_index);
  auto segment_num_str = ss.str();
  // and the block number within the segment
  std::stringstream blk_strm;
  blk_strm << std::setw(kMAX_LEFTPADDED_ZER0S) << std::setfill('0') << chain.getSegmentHeightAt(block_index);
  auto block_num_str = blk_strm.str();

  fs::path block_path(working_dir / shard_name / segment_num_str / (block_num_str + kBLOCK_SUFFIX));

  return block_path;
}

BlockIOFS::BlockIOFS(const std::string& chain_name,
                     const std::string& base_path,
                     const std::string& shard_uri)
: chain_(chain_name)
, base_path_(base_path)
, shard_uri_(shard_uri)
{

}

void BlockIOFS::writeBlock(FinalBlockSharedPtr block) {
  fs::path seg_dir(base_path_ / shard_uri_
                          / std::to_string(chain_.getCurrentSegmentIndex()));

  //fs::path dir_path(seg_dir);
  //if (!is_directory(dir_path)) fs::create_directory(dir_path);
  auto out_file = GetStandardBlockPath(chain_, shard_uri_, base_path_, chain_.size());
  std::ofstream block_file(out_file.string(), std::ios::out | std::ios::binary);
  if (!is_directory(out_file)) fs::create_directory(out_file.branch_path());

  if (block_file.is_open()) {
    auto canonical = block->getCanonical();
    block_file.write(reinterpret_cast<char*>(canonical.data()), canonical.size());
    block_file.close();
    LOG_DEBUG << "Wrote to " << out_file << "'.";
  } else {
    LOG_ERROR << "Failed to open output file '" << out_file << "'.";
  }
}

} // namespace Devv
