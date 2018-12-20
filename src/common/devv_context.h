/*
 * DevvContext.h runtime constants for a Devv shard.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef COMMON_DEVV_CONTEXT_H_
#define COMMON_DEVV_CONTEXT_H_

#include <string>
#include <vector>
#include <sys/stat.h>

#include "common/devv_types.h"

namespace Devv {

//Consensus Params
static const int kACTIVATION_ROUNDS = 334;
static const unsigned int kPROPOSAL_TIMEOUT = 60000;
static const int kVALIDATION_PERCENT = 51;

static const unsigned int kSYNC_PORT_BASE = 55330;
static const std::string kURI_PREFIX ="RemoteURI-";
static const std::string kSHARD_PREFIX = "shard-";

static std::string get_shard_uri(int current_shard) {
  return (kSHARD_PREFIX + std::to_string(current_shard));
}

enum eDebugMode {off, on, toy, perf};

struct DevvContext {

  /**
   * Constructor
   */
  DevvContext(unsigned int current_node,
              unsigned int current_shard,
              eAppMode mode,
              const std::string& inn_key_path,
              const std::string& node_key_path,
              const std::string& key_pass,
              unsigned int batch_size=10000,
              unsigned int max_wait=0)
      : current_node_(current_node)
      , current_shard_(current_shard)
      , app_mode_(mode)
      , uri_(get_uri_from_index(current_node+current_shard*peer_count_))
      , inn_keys_(inn_key_path)
      , node_keys_(node_key_path)
      , key_pass_(key_pass)
      , batch_size_(batch_size)
      , max_wait_(max_wait)
  {
  }

  size_t get_peer_count() const { return peer_count_; }
  unsigned int  get_current_node() const { return current_node_+current_shard_*peer_count_; }
  unsigned int  get_current_shard() const { return current_shard_; }
  eAppMode get_app_mode() const { return app_mode_; }
  std::string get_uri() const { return uri_; }

  std::string get_uri_from_index(unsigned int node_index) const {
    return (kURI_PREFIX + std::to_string(node_index));
  }

  static std::string get_shard_uri(int current_shard) {
    return (kSHARD_PREFIX + std::to_string(current_shard));
  }

  std::string get_shard_uri() const {
    return (kSHARD_PREFIX + std::to_string(current_shard_));
  }

  std::string get_inn_key_path() const { return inn_keys_; }
  std::string get_node_key_path() const { return node_keys_; }
  std::string get_key_password() const { return key_pass_; }
  unsigned int get_batch_size() const {return batch_size_; }
  unsigned int get_max_wait() const {return max_wait_; }

private:
  /** Number of connected peers */
  const size_t peer_count_ = 3;

  // Node index of this process
  unsigned int current_node_;

  // Shard Index of this process
  unsigned int current_shard_;

  // Process mode
  eAppMode app_mode_ = scan;

  // This processes uri
  std::string uri_;

  //key file paths
  std::string inn_keys_;
  std::string node_keys_;

  //AES private key password
  std::string key_pass_;

  unsigned int batch_size_;
  unsigned int max_wait_;
};

} /* namespace Devv */

#endif /* COMMON_DEVV_CONTEXT_H_ */
