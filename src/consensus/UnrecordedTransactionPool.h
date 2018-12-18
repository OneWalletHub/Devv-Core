/*
 * UnrecordedTransactionPool.h
 * tracks Transactions that have not yet been encoded in the blockchain.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <atomic>
#include <map>
#include <vector>
#include <mutex>
#include <ostream>

#include "concurrency/TransactionCreationManager.h"
#include "concurrency/thread_tools.h"

#include "primitives/FinalBlock.h"
#include "primitives/factories.h"
#include "common/logger.h"

namespace Devv
{

typedef std::pair<uint8_t, TransactionPtr> SharedTransaction;
typedef std::map<Signature, SharedTransaction> TxMap;

/**
 * A pool of unrecorded transactions.
 */
class UnrecordedTransactionPool {
 public:

  /**
   * Constructor
   * @param prior the current chainstate
   * @param mode Tier1 or Tier2 mode
   * @param max_tx_per_block Maximum number of transactions per block
   */
  UnrecordedTransactionPool(const ChainState& prior, eAppMode mode, size_t max_tx_per_block);

  /**
   * Deleted copy constructor
   * @param other
   */
  UnrecordedTransactionPool(const UnrecordedTransactionPool& other) = delete;

  /**
   * Adds Transactions to this pool.
   *  @note if the Transaction is invalid it will not be added,
   *    but other valid transactions will be added.
   *  @note if the Transaction is valid, but is already in the pool,
   *        its reference count will be incremented.
   *  @param serial a devv-encoded representation of the transactions
   *  @param keys a KeyRing that provides keys for signature verification
   *  @return true iff all Transactions are valid and in the pool
   */
  bool addTransactions(const std::vector<byte>& serial, const KeyRing& keys);

  /**
   * Adds Transactions to this pool.
   *  @note if the Transaction is invalid it will not be added,
   *    but other valid transactions will be added.
   *  @note if the Transaction is valid, but is already in the pool,
   *  @param txs a vector of Transactions to add.
   *  @param keys a KeyRing that provides keys for signature verification
   *  @return true iff all Transactions are valid and in the pool
   */
  bool addTransactions(std::vector<TransactionPtr>& txs, const KeyRing& keys);

  /**
   * Adds and verifies Transactions for this pool.
   *  @note if *any* Transaction is invalid or unsound,
   *    this function breaks and returns false,
   *    so other Transactions may not be added or verified
   *  @param txs a vector of Transactions to add and verify
   *  @param state the chain state to validate against
   *  @param keys a KeyRing that provides keys for signature verification
   *  @param summary the Summary to update
   *  @return true iff all Transactions are valid and in the pool
   *  @return false if any of the Transactions are invalid or unsound
   */
  bool addAndVerifyTransactions(std::vector<TransactionPtr> txs,
                                ChainState& state,
                                const KeyRing& keys,
                                Summary& summary);

  /**
   * Returns a JSON string representing these Transactions
   *  @note pointer counts are not preserved.
   *  @return a JSON string representing these Transactions.
   */
  std::string getJSON() const;

  /**
   * Returns a bytestring of these Transactions
   * @note This method creates a txs_mutex_ lock guard
   * @note An empty vector of bytes is created to hold the transactions.
   *       For each transaction, a copy of the canonical form of the transaction
   *       is created and then is insert()ed onto the end of the vector of
   *       transactions.
   * @return a bytestring of these Transactions
   */
  std::vector<byte> getCanonicalTransactions() const;

  /**
   * Checks if this pool has pending Transactions
   * @return true, iff this pool has pending Transactions
   */
  bool hasPendingTransactions() const;

  /**
   * Returns the number of pending transactions
   * @return the number of pending Transactions in this pool
   */
  size_t numPendingTransactions() const;

  /**
   *  Create a new ProposedBlock based on pending Transaction in this pool.
   *  Locks this pool for proposals.
   *  @param prev_hash - the hash of the previous block
   *  @param prior_state - the chainstate prior to this proposal
   *  @param keys - the directory of Addresses and EC keys
   *  @param context - the Devv context of this shard
   *  @return true, iff this pool created a new ProposedBlock
   *  @return false, if anything went wrong
   */
  bool proposeBlock(const Hash& prev_hash,
                    const ChainState& prior_state,
                    const KeyRing& keys,
                    const DevvContext& context);

  /**
   * Indicates whether or not this validator has a proposed block
   * that is awaiting validation.
   * @note uses atomic<bool> indicator
   * @return true, iff this pool has an active ProposedBlock
   */
  bool hasActiveProposal();

  /**
   * Sets proposal state - true if this validator has proposed a block
   * and is awaiting validation.
   * @param is_active_proposal true iff a ProposedBlock is pending validation
   */
  void setHasActiveProposal(bool is_active_proposal) {
    has_active_proposal_= is_active_proposal;
  }

  /**
   *  @return a binary representation of this pool's ProposedBlock
   */
  std::vector<byte> getProposal();

  /**
   *  Update this pool's ProposedBlock based on a new FinalBlock.
   *  @note a new proposal may be generated
   *        if proposed transactions are now invalid
   *  @param prev_hash - the hash of the highest FinalBlock
   *  @param prior - the chainstate of the highest FinalBlock
   *  @param keys - the directory of Addresses and EC keys
   *  @param context - the context for this node/shard
   *  @return true, iff this pool updated its ProposedBlock
   *  @return false, if anything went wrong
   */
  bool reverifyProposal(const Hash& prev_hash,
                        const ChainState& prior,
                        const KeyRing& keys,
                        const DevvContext& context);

  /**
   *  Checks a remote proposal against this pool's recent transactions
   *  @param proposed - the remote ProposedBlock
   *  @return true, iff no transactions in the remote proposal match
   *                transactions recently finalized in this pool
   *  @return false, a duplicate transaction was detected
   */
  bool isRemoteProposalDuplicateFree(const ProposedBlock& proposed);

  /**
   * Set the proposal time to the current time
   * @return the proposal time (the current time)
   */
  uint64_t setProposalTime() {
    proposal_time_ = GetMillisecondsSinceEpoch();
    return proposal_time_;
  }

  /**
   *  Checks a remote validation against this pool's ProposedBlock
   *  @param remote - a binary representation of the remote Validation
   *  @param context - the Devv context of this shard
   *  @return true, iff the Validation validates this pool's ProposedBlock
   *  @return false, otherwise including if this pool has no ProposedBlock,
   *                 the Validation is for a different ProposedBlock,
   *                 or the Validation signature did not verify
   */
  bool checkValidation(InputBuffer& buffer, const DevvContext& context);

  /**
   *  Create a new FinalBlock based on this pool's ProposedBlock
   *  @pre ensure this HasProposal() == true before calling this function
   *  @note nullifies this pool's ProposedBlock as a side-effect
   *  @return a FinalBlock based on this pool's ProposedBlock
   */
  const FinalBlock finalizeLocalBlock();

  /**
   *  Create a new FinalBlock based on a remotely serialized FinalBlock
   *  @param serial - a binary representation of the FinalBlock
   *  @param prior - the chain state prior to this new FinalBlock
   *  @param keys - the directory of Addresses and EC keys
   *  @return a FinalBlock based on the remote data provided
   */
  const FinalBlock finalizeRemoteBlock(InputBuffer& buffer,
                                       const ChainState prior,
                                       const KeyRing& keys);

  /**
   * Remove unreferenced Transactions from the pool.
   *  @return the number of Transactions removed.
   */
  int GarbageCollect();

  /**
   *  @return the elapsed time since this pool last received a Transaction
   */
  double getElapsedTime() {
    return timer_.elapsed();
  }

 /**
  *  @return the tool to create Transactions in parallel
  */
  TransactionCreationManager& getTransactionCreationManager() {
    return(tcm_);
  }

 /**
  * Return the mode (T1 or T2) of this object
  * @return the validator mode, which determines the type of Transactions handled.
  */
  eAppMode getMode() const {
    return mode_;
  }

  /**
   * Acquire a lock to ensure permission to propose. Once the lock
   * is acquired it must be locked to ensure the calling thread has
   * the go ahead to propose.
   *
   * @return a unique_ptr to a ILock
   */
  std::unique_ptr<ILock> acquireProposalPermissionLock() const {
    return proposal_permission_lock_->clone();
  }

  /**
   * Mostly used by test cases to swap the UniqueLock with a test lock
   * @param lock A new lock with which to swap the permission lock
   */
  void setProposalPermissionLock(std::unique_ptr<ILock> lock) {
    proposal_permission_lock_.swap(lock);
  }

  /**
   * Checks whether the final_block_processing_ flag has been set. Used
   * to preempt a proposal if a new FinalBlock arrives.
   * @return true iff a FinalBlock is being processed
   */
  bool isNewFinalBlockProcessing() const {
    return is_new_final_block_processing_;
  }

  /**
   * Set the final_block_processing_ flag to true. This will preempt
   * a thread that is in the process of proposing when a FinalBlock
   * arrives and is being handled.
   *
   * @param processing_now
   */
  void indicateNewFinalBlock(bool processing_now = true) {
    is_new_final_block_processing_ = processing_now;
  }

  /**
   * Get a mutex to coordinate shared access to local
   * data
   */
  /**
   * Acquire full lock to coordinate shared access to local
   * data.
   * @note This is viewed as a temporary fix. It hurts performance
   * but guarantees that only one thread is accessing the UTXPool
   * at a time.
   * @return
   */
  std::unique_ptr<ILock> acquireFullLock(bool lock_on_acquire = true) const {
    return full_lock_->clone();
  }

  void setFullLock(std::unique_ptr<ILock> lock) {
    full_lock_.swap(lock);
  }

 private:
  TxMap txs_;
  std::map<Signature, uint64_t> recent_txs_;

  mutable std::mutex txs_mutex_;

  /// True if this validator has an active proposal currently pending validation
  std::atomic<bool> has_active_proposal_ = ATOMIC_VAR_INIT(false);

  /// True iff
  std::atomic<bool> is_new_final_block_processing_ = ATOMIC_VAR_INIT(false);

  /// Determines which thread has permission to propose
  mutable std::unique_ptr<ILock> proposal_permission_lock_ = std::make_unique<UniqueLock>();

  ProposedBlock pending_proposal_;
  mutable std::mutex pending_proposal_mutex_;
  uint64_t proposal_time_ = 0;

  // Total number of transactions that have been
  // added to the transaction map
  size_t num_cum_txs_ = 0;
  size_t max_tx_per_block_ = 10000;

  // Time since starting
  Timer timer_;
#ifdef MTR_ENABLED
  std::unique_ptr<MTRScopedTrace> trace_;
#endif
  TransactionCreationManager tcm_;
  eAppMode mode_;

  /// Temporary/test mutex to lock all callbacks and force serial
  /// execution
  //mutable std::mutex full_mutex_;

  mutable std::unique_ptr<ILock> full_lock_ = std::make_unique<UniqueLock>();

  /**
   *  Create a new ProposedBlock based on pending Transaction in this pool
   *  @param prev_hash - the hash of the previous block
   *  @param prior_state - the chainstate prior to this proposal
   *  @param keys - the directory of Addresses and EC keys
   *  @param context - the Devv context of this shard
   *  @return true, iff this pool created a new ProposedBlock
   *  @return false, if anything went wrong
   */
  bool proposeBlock_Internal(const Hash& prev_hash,
                    const ChainState& prior_state,
                    const KeyRing& keys,
                    const DevvContext& context);

  /**
   * Verifies Transactions for this pool.
   *  @note this implementation is greedy in selecting Transactions
   *  @params state the chain state to validate against
   *  @params keys a KeyRing that provides keys for signature verification
   *  @params summary the Summary to update
   *  @return a vector of unrecorded valid transactions
   */
  std::vector<TransactionPtr> collectValidTransactions(const ChainState& state
      , const KeyRing& keys, const Summary& pre_sum, const DevvContext& context);

  /**
   * Reverifies Transactions for this pool.
   *  @note this function modifies pending_proposal_
   *  @note if this returns false, a new proposal must be created
   *  @params state the chain state to validate against
   *  @params keys a KeyRing that provides keys for signature verification
   *  @return true iff, all transactions are valid wrt provided chainstate
   *  @return false otherwise, a new proposal must be created
   */
  bool reverifyTransactions(const ChainState& prior, const KeyRing& keys);

  /**
   * Removes Transaction from this pool
   *  @pre not thread-safe, call from within a mutex guard
   *  @return true if Transaction was removed
   *  @return false otherwise
   */
  bool removeTransaction(const Signature& sig);

  /**
   * Removes invalid transactions from this pool
   *  @pre not thread-safe, call from within a mutex guard
   *  @return true if all invalid transactions in the block were removed
   *  @return false if possible that not all invalid transactions removed
   */
  bool removeInvalidTransactions(const ChainState& state
      , const KeyRing& keys, const Summary& summary);

  /**
   * Removes Transactions in a Proposed/FinalBlock from this pool
   *  @param block - the Block containing Transactions to remove
   *  @pre this should only be called for a Block that is finalizing
   *  @pre do not call if there is any chance a Block will not finalize
   *  @return true iff, all transactions in the block were removed
   *  @return false otherwise
   */
  template <typename Block>
  bool removeTransactions(const Block& block) {
    std::lock_guard<std::mutex> guard(txs_mutex_);
    auto txs_size = txs_.size();
    for (auto const& item : block.getTransactions()) {
      if (txs_.erase(item->getSignature()) == 0) {
        LOG_WARNING << "removeTransactions(): ret = 0, transaction not found: " << item->getSignature().getJSON();
      } else {
        LOG_DEBUG << "removeTransactions(): erase returned 1: " << item->getSignature().getJSON();
      }
      recent_txs_.insert(std::pair<Signature, uint64_t>(item->getSignature(), GetMillisecondsSinceEpoch()));
    }
    LOG_DEBUG << "removeTransactions: (to_remove/size_pre/size_post) ("
              << block.getNumTransactions() << "/"
              << txs_size << "/"
              << txs_.size() << ")";
    return (block.getNumTransactions() == txs_size - txs_.size());
  }

  /**
   * Finalizes a ProposedBlock
   *  @param proposal - the ProposedBlock to finalize
   *  @return the FinalBlock based on the provided ProposedBlock
   */
  const FinalBlock finalizeBlock(const ProposedBlock& proposal);
};

} //end namespace Devv
