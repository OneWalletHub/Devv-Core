/**
 * ConsensusController.h controls callbacks for the
 * consensus process between Devv validators.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "types/DevvMessage.h"
#include "consensus/UnrecordedTransactionPool.h"
#include "consensus/blockchain.h"
#include "consensus/chainstate.h"

namespace Devv {

class ConsensusController {
typedef std::function<bool(DevvMessageUniquePtr ptr,
                           const DevvContext &context,
                           const KeyRing &keys,
                           Blockchain &final_chain,
                           UnrecordedTransactionPool &utx_pool,
                           std::function<void(DevvMessageUniquePtr)> callback)> FinalBlockCallback;

typedef std::function<bool(DevvMessageUniquePtr ptr,
                           const DevvContext &context,
                           const KeyRing &keys,
                           Blockchain &final_chain,
                           UnrecordedTransactionPool &utx_pool,
                           std::function<void(DevvMessageUniquePtr)> callback)> ProposalBlockCallback;

typedef std::function<bool(DevvMessageUniquePtr ptr,
                           const DevvContext &context,
                           Blockchain &final_chain,
                           UnrecordedTransactionPool &utx_pool,
                           std::function<void(DevvMessageUniquePtr)> callback)> ValidationBlockCallback;
 public:
  ConsensusController(const KeyRing& keys,
                      DevvContext& context,
                      const ChainState& prior,
                      Blockchain& final_chain,
                      UnrecordedTransactionPool& utx_pool,
                      eAppMode mode);

  void registerOutgoingCallback(DevvMessageCallback callback) {
    outgoing_callback_ = callback;
  }
  /**
   * Process a consensus worker message.
   */
  void consensusCallback(DevvMessageUniquePtr ptr);

 private:
  const KeyRing& keys_;
  DevvContext& context_;
  Blockchain& final_chain_;
  UnrecordedTransactionPool& utx_pool_;
  eAppMode mode_;

  mutable std::mutex mutex_;

  /// A callback to send the analyzed DevvMessage
  DevvMessageCallback outgoing_callback_;

  FinalBlockCallback final_block_cb_;
  ProposalBlockCallback proposal_block_cb_;
  ValidationBlockCallback validation_block_cb_;

};

} // namespace Devv
