/*
 * BlockchainModule.h manages this node in the devcash network.
 *
 *  Created on: Mar 20, 2018
 *      Author: Nick Williams
 */
#pragma once

#include <string>
#include <vector>

#include "concurrency/ConsensusController.h"
#include "concurrency/InternetworkController.h"
#include "concurrency/ValidatorController.h"
#include "concurrency/ThreadGroup.h"
#include "io/message_service.h"
#include "modules/ParallelExecutor.h"
#include "modules/ModuleInterface.h"
#include "io/message_service.h"

namespace Devcash {

/**
 * The Blockchain Module implements the blockchain logic
 */
class BlockchainModule : public ModuleInterface {
  typedef std::unique_ptr<ParallelExecutor<ConsensusController>> ThreadedConsensusPtr;
  typedef std::unique_ptr<ParallelExecutor<InternetworkController>> ThreadedInternetworkPtr;
  typedef std::unique_ptr<ParallelExecutor<ValidatorController>> ThreadedValidatorPtr;

 public:
  BlockchainModule(io::TransactionServer &server,
                  io::TransactionClient &client,
                  const KeyRing &keys,
                  const ChainState &prior,
                  eAppMode mode,
                  DevcashContext &context,
                  size_t max_tx_per_block);

  /**
   * Move constructor
   * @param other
   */
  BlockchainModule(BlockchainModule&& other) = default;

  /**
   * Default move-assignment operator
   * @param other
   * @return
   */
  BlockchainModule& operator=(BlockchainModule&& other) = default;

  virtual ~BlockchainModule() {}

  /**
   * Create a new Blockchain module
   */
  static std::unique_ptr<BlockchainModule> Create(io::TransactionServer &server,
                                io::TransactionClient &client,
                                const KeyRing &keys,
                                const ChainState &prior,
                                eAppMode mode,
                                DevcashContext &context,
                                size_t max_tx_per_block);

  /** Initialize devcoin core: Basic context setup.
   *  @note Do not call Shutdown() if this function fails.
   *  @pre Parameters should be parsed and config file should be read.
   */
  void init() override;

  /**
   * Initialization sanity checks: ecc init, sanity checks, dir lock.
   * @note Do not call Shutdown() if this function fails.
   * @pre Parameters should be parsed and config file should be read.
   */
  void performSanityChecks() override;

  /**
   * Stop any running threads and shutdown the module
   */
  void shutdown();

  /**
   * Devcash core main initialization.
   * @note Call Shutdown() if this function fails.
   */
  void start();

  void handleMessage(DevcashMessageUniquePtr message);

  Blockchain& getFinalChain() { return final_chain_; }

  const Blockchain& getFinalChain() const { return final_chain_; }

  UnrecordedTransactionPool& getTransactionPool() { return utx_pool_; }

  const UnrecordedTransactionPool& getTransactionPool() const { return utx_pool_; }

 private:
  io::TransactionServer &server_;
  io::TransactionClient &client_;

  const KeyRing &keys_;
  const ChainState &prior_;
  eAppMode mode_;
  DevcashContext &app_context_;

  Blockchain final_chain_;
  UnrecordedTransactionPool utx_pool_;

  ConsensusController consensus_controller_;
  InternetworkController internetwork_controller_;
  ValidatorController validator_controller_;

  ThreadedConsensusPtr consensus_executor_ = nullptr;
  ThreadedInternetworkPtr internetwork_executor_ = nullptr;
  ThreadedValidatorPtr validator_executor_ = nullptr;
};

} //end namespace Devcash
