/*
 * ValidatorController.h controls worker threads for the Devcash protocol.
 *
 *  Created on: Mar 21, 2018
 *      Author: Nick Williams
 */
#ifndef CONCURRENCY_DEVCASHCONTROLLER_H_
#define CONCURRENCY_DEVCASHCONTROLLER_H_

#include <condition_variable>
#include <mutex>

#include "consensus/KeyRing.h"
#include "consensus/UnrecordedTransactionPool.h"
#include "consensus/blockchain.h"
#include "consensus/chainstate.h"
#include "io/message_service.h"

namespace Devcash {

class DevcashControllerWorker;

typedef std::function<bool(DevcashMessageUniquePtr ptr,
                           Blockchain &final_chain,
                           DevcashContext context,
                           const KeyRing &keys,
                           const UnrecordedTransactionPool &,
                           uint64_t &remote_blocks)> BlocksSinceCallback;

typedef std::function<bool(DevcashMessageUniquePtr ptr,
                           Blockchain &final_chain,
                           const DevcashContext &context,
                           const KeyRing &keys,
                           std::function<void(DevcashMessageUniquePtr)> callback)> BlocksSinceRequestCallback;

typedef std::function<bool(DevcashMessageUniquePtr ptr,
                           const DevcashContext &context,
                           const KeyRing &keys,
                           Blockchain &final_chain,
                           UnrecordedTransactionPool &utx_pool,
                           std::function<void(DevcashMessageUniquePtr)> callback)> FinalBlockCallback;

typedef std::function<bool(DevcashMessageUniquePtr ptr,
                           const DevcashContext &context,
                           const KeyRing &keys,
                           Blockchain &final_chain,
                           TransactionCreationManager &tcm,
                           std::function<void(DevcashMessageUniquePtr)> callback)> ProposalBlockCallback;

typedef std::function<DevcashMessageUniquePtr(const KeyRing& keys,
                                           Blockchain& final_chain,
                                           UnrecordedTransactionPool& utx_pool,
                                           const DevcashContext& context)> TransactionAnnouncementCallback;

typedef std::function<bool(DevcashMessageUniquePtr ptr,
                           const DevcashContext &context,
                           Blockchain &final_chain,
                           UnrecordedTransactionPool &utx_pool,
                           std::string working_dir,
                           std::function<void(DevcashMessageUniquePtr)> callback)> ValidationBlockCallback;



/**
 * Holds function pointers to message callbacks
 */
struct DevcashMessageCallbacks {
  BlocksSinceCallback blocks_since_cb;
  BlocksSinceRequestCallback blocks_since_request_cb;
  FinalBlockCallback final_block_cb;
  ProposalBlockCallback proposal_block_cb;
  TransactionAnnouncementCallback transaction_announcement_cb;
  ValidationBlockCallback validation_block_cb;
};

class ValidatorController {
 public:
  ValidatorController(io::TransactionServer& server,
                    io::TransactionClient& peer_client,
                    size_t validator_count,
                    size_t consensus_count,
                    size_t batch_size,
                    const KeyRing& keys,
                    DevcashContext& context,
                    const ChainState& prior,
                    eAppMode mode,
                    const std::string& working_dir,
                    const std::string& stop_file);

  ~ValidatorController();

  std::vector<std::vector<byte>> loadTransactions();

  /**
   * Start the workers and comm threads
   */
  void Start();

  /** Stops all threads used by this controller.
   * @note This function may block.
   */
  void StopAll();

  /**
   * Push a message to the consensus workers.
   */
  void pushConsensus(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Push a message to the validator workers.
   */
  void pushValidator(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Push a message to the inter-shard communication workers.
   */
  void pushShardComms(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Process a consensus worker message.
   */
  void ConsensusCallback(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Process a validator worker message.
   */
  void ValidatorCallback(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Initializes message callback functions
   * @param[in] callbacks
   */
  void setMessageCallbacks(DevcashMessageCallbacks callbacks) {
    message_callbacks_ = callbacks;
  }

  /**
   * Process a inter-shard communciation worker message.
   */
  void ShardCommsCallback(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Process a consensus toy worker message.
   */
  void ConsensusToyCallback(std::unique_ptr<DevcashMessage> ptr);

  /**
   * Process a validator toy worker message.
   */
  void ValidatorToyCallback(std::unique_ptr<DevcashMessage> ptr);

 private:
  io::TransactionServer& server_;
  io::TransactionClient& peer_client_;
  const int validator_count_;
  const int consensus_count_;
  size_t shutdown_counter_ = 0;
  const KeyRing& keys_;
  DevcashContext& context_;
  Blockchain final_chain_;
  UnrecordedTransactionPool utx_pool_;
  eAppMode mode_;
  std::string working_dir_;
  std::string stop_file_;

  // Pointer because incomplete type
  DevcashControllerWorker* workers_ = nullptr;
  bool validator_flipper_ = true;
  bool consensus_flipper_ = true;
  bool shutdown_ = false;
  uint64_t waiting_ = 0;
  mutable std::mutex mutex_;
  uint64_t remote_blocks_ = 0;
  size_t input_blocks_ = 0;

  DevcashMessageCallbacks message_callbacks_;
};

} /* namespace Devcash */

#endif /* CONCURRENCY_DEVCASHCONTROLLER_H_ */