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
  fs::path shard_path(working_dir / shard_name);
  return GetBlockPath(shard_path,
                      chain.getSegmentIndexAt(block_index),
                      chain.getSegmentHeightAt(block_index));
}

boost::filesystem::path GetBlockPath(const fs::path& shard_path,
                                     size_t segment_index,
                                     size_t segment_height) {
  // Set the segment number
  std::stringstream ss;
  ss << std::setw(kMAX_LEFTPADDED_ZER0S) << std::setfill('0') << segment_index;
  auto segment_num_str = ss.str();
  // and the block number within the segment
  std::stringstream blk_strm;
  blk_strm << std::setw(kMAX_LEFTPADDED_ZER0S) << std::setfill('0') << segment_height;
  auto block_num_str = blk_strm.str();

  fs::path block_path(shard_path / segment_num_str / (block_num_str + kBLOCK_SUFFIX));

  return block_path;
}

BlockIOFS::BlockIOFS(const Blockchain& chain,
                     const std::string& base_path,
                     const std::string& shard_uri)
: chain_(chain)
, base_path_(base_path)
, shard_uri_(shard_uri)
{

}

void BlockIOFS::writeBlock(size_t block_index) {
  auto out_file = GetStandardBlockPath(chain_, shard_uri_, base_path_, block_index);
  if (!is_directory(out_file)) fs::create_directory(out_file.branch_path());
  std::ofstream block_file(out_file.string(), std::ios::out | std::ios::binary);

  if (block_file.is_open()) {
    auto canonical = chain_.at(block_index)->getCanonical();
    block_file.write(reinterpret_cast<char*>(canonical.data()), canonical.size());
    block_file.close();
    LOG_DEBUG << "Wrote to " << out_file << "'.";
  } else {
    LOG_ERROR << "Failed to open output file '" << out_file << "'.";
  }
}

} // namespace Devv
