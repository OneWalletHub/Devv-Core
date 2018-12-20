/**
 * InternetworkController.h controls callbacks for
 * communications to and from processes outside the Devv shard peers.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <mutex>

#include "types/DevvMessage.h"
#include "consensus/blockchain.h"
#include "consensus/UnrecordedTransactionPool.h"
#include "consensus/chainstate.h"

namespace Devv {

class InternetworkController {
  /// typedef BlocksSinceCallback
  typedef std::function<bool(DevvMessageUniquePtr ptr,
                             Blockchain &final_chain,
                             DevvContext context,
                             const KeyRing &keys,
                             const UnrecordedTransactionPool &,
                             uint64_t &remote_blocks)> BlocksSinceCallback;
  /// typedef BlocksSinceRequestCallback
  typedef std::function<bool(DevvMessageUniquePtr ptr,
                             Blockchain &final_chain,
                             const DevvContext &context,
                             const KeyRing &keys,
                             std::function<void(DevvMessageUniquePtr)> callback)> BlocksSinceRequestCallback;

 public:
  InternetworkController(const KeyRing& keys,
                      DevvContext& context,
                      const ChainState& prior,
                      Blockchain& final_chain,
                      UnrecordedTransactionPool& utx_pool,
                      eAppMode mode);


  void registerOutgoingCallback(DevvMessageCallback callback) {
    outgoing_callback_ = callback;
  }
  void messageCallback(DevvMessageUniquePtr ptr);

 private:
  const KeyRing& keys_;
  DevvContext& context_;
  Blockchain& final_chain_;
  UnrecordedTransactionPool& utx_pool_;
  eAppMode mode_;
  size_t remote_blocks_;

  std::mutex blockchain_mutex_;
  std::mutex utx_mutex_;
  /// A callback to send the analyzed DevvMessage
  DevvMessageCallback outgoing_callback_;

  BlocksSinceCallback blocks_since_cb_;
  BlocksSinceRequestCallback blocks_since_request_cb_;

};

} // namespace Devv
