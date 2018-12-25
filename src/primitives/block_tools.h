/*
 * primitives/block_tools.h defines tools for
 * utilizing blocks
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <boost/filesystem/path.hpp>
#include "primitives/binary_tools.h"
#include "primitives/blockchain.h"

namespace Devv {

/** Checks if two chain state maps contain the same state
 * @return true if the maps have the same state
 * @return false otherwise
 */
bool CompareChainStateMaps(const std::map<Address, std::map<uint64_t, int64_t>>& first,
                           const std::map<Address, std::map<uint64_t, int64_t>>& second);

/** Dumps the map inside a chainstate object into a human-readable JSON format.
 * @return a string containing a description of the chain state.
 */
std::string WriteChainStateMap(const std::map<Address, std::map<uint64_t, int64_t>>& map);

/**
 * Create a Tier1Transaction from a FinalBlock
 * Throws on error
 *
 * @param block Valid FinalBlock
 * @param keys KeyRing for new transaction
 * @return unique_ptr to new Tier1Transaction
 */
Tier1TransactionPtr CreateTier1Transaction(const FinalBlock& block, const KeyRing& keys);

/**
 * Sign the summary. This is a simple helper function to
 * sign the hash of the Summary.
 *
 * @param summary Summary to sign
 * @param keys Keys to sign with
 * @return Signature
 */
Signature SignSummary(const Summary& summary, const KeyRing& keys);


/**
 * @param shard - the name of the directory for this shard
 * @param block - the height of the block to generate a path for
 * @param working_dir - the working directory to create a path from
 * @param separator - the preferred path separator for this file system
 * @return a standard path where a particular block would be stored relative to the working directory.
 */
boost::filesystem::path GetStandardBlockPath(const Blockchain& chain,
                                             const std::string& shard_name,
                                             const boost::filesystem::path& working_dir,
                                             size_t block_index);

boost::filesystem::path GetBlockPath(const boost::filesystem::path& shard_path,
                                     size_t segment_index,
                                     size_t segment_height);

/**
 * Interfaces with standard POSIX filesystems
 */
class BlockIOFS {
 public:
  BlockIOFS(const std::string& chain_name,
            const std::string& base_path,
            const std::string& shard_uri);

  virtual ~BlockIOFS() = default;

  void writeBlock(FinalBlockSharedPtr block);

 private:
  // Holds the blockchain
  Blockchain chain_;
  // Location of blocks
  boost::filesystem::path base_path_;
  // The name of this shard
  std::string shard_uri_;
};

} // namespace Devv
