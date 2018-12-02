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

#include "concurrency/TransactionCreationManager.h"
#include "primitives/FinalBlock.h"
#include "primitives/factories.h"
#include "common/logger.h"

namespace Devv
{

typedef std::pair<uint8_t, TransactionPtr> SharedTransaction;
typedef std::map<Signature, SharedTransaction> TxMap;

class UnrecordedTransactionPool {
 public:

  /** Constructors */
  UnrecordedTransactionPool(const ChainState& prior, eAppMode mode
     , size_t max_tx_per_block)
     : txs_()
    , pending_proposal_(ProposedBlock::Create(prior))
    , max_tx_per_block_(max_tx_per_block)
    , tcm_(mode)
    , mode_(mode)
  {
    LOG_DEBUG << "UnrecordedTransactionPool(const ChainState& prior)";
  }

  UnrecordedTransactionPool(const UnrecordedTransactionPool& other) = delete;

  /** Adds Transactions to this pool.
   *  @note if the Transaction is invalid it will not be added,
   *    but other valid transactions will be added.
   *  @note if the Transaction is valid, but is already in the pool,
   *        its reference count will be incremented.
   *  @param txs a vector of Transactions to add.
   *  @params keys a KeyRing that provides keys for signature verification
   *  @return true iff all Transactions are valid and in the pool
  */
  bool addTransactions(const std::vector<byte>& serial, const KeyRing& keys) {
    LOG_DEBUG << "addTransactions(const std::vector<byte>& serial, const KeyRing& keys)";
    MTR_SCOPE_FUNC();
    std::vector<TransactionPtr> temp;
    InputBuffer buffer(serial);
    while (buffer.getOffset() < buffer.size()) {
      auto tx = CreateTransaction(buffer, keys, mode_);
      temp.push_back(std::move(tx));
    }
    return addTransactions(temp, keys);
  }

  /** Adds Transactions to this pool.
   *  @note if the Transaction is invalid it will not be added,
   *    but other valid transactions will be added.
   *  @note if the Transaction is valid, but is already in the pool,
   *  @param txs a vector of Transactions to add.
   *  @params keys a KeyRing that provides keys for signature verification
   *  @return true iff all Transactions are valid and in the pool
  */
  bool addTransactions(std::vector<TransactionPtr>& txs, const KeyRing& keys) {
    LOG_DEBUG << "addTransactions(std::vector<Transaction> txs, const KeyRing& keys)";
    MTR_SCOPE_FUNC();
    std::lock_guard<std::mutex> guard(txs_mutex_);
    bool all_good = true;
    CASH_TRY {
      int counter = 0;
      for (TransactionPtr& item : txs) {
        Signature sig = item->getSignature();
        auto it = txs_.find(sig);
        if (it != txs_.end()) {
          it->second.first++;
          LOG_DEBUG << "Transaction already in UTX pool, increment reference count.";
        } else if (item->isSound(keys)) {
          SharedTransaction pair((uint8_t) 1, std::move(item));
          txs_.insert(std::pair<Signature, SharedTransaction>(sig, std::move(pair)));
          if (num_cum_txs_ == 0) {
            LOG_NOTICE << "addTransactions(): First transaction added to TxMap";
            timer_.reset();
#ifdef MTR_ENABLED
            trace_ = std::make_unique<MTRScopedTrace>("timer", "lifetime1");
#endif
          }
          num_cum_txs_++;
          counter++;
        } else { //tx is unsound
          LOG_DEBUG << "Transaction is unsound.";
          all_good = false;
        }
      }
      LOG_INFO << "Added "+std::to_string(counter)+" sound transactions.";
      LOG_INFO << std::to_string(txs_.size())+" transactions pending.";
    } CASH_CATCH (const std::exception& e) {
      LOG_FATAL << FormatException(&e, "UnrecordedTransactionPool.addTransactions()");
      all_good = false;
    }
    return all_good;
  }

  /** Adds and verifies Transactions for this pool.
   *  @note if *any* Transaction is invalid or unsound,
   *    this function breaks and returns false,
   *    so other Transactions may not be added or verified
   *  @param txs a vector of Transactions to add and verify
   *  @params state the chain state to validate against
   *  @params keys a KeyRing that provides keys for signature verification
   *  @params summary the Summary to update
   *  @return true iff all Transactions are valid and in the pool
   *  @return false if any of the Transactions are invalid or unsound
  */
  bool addAndVerifyTransactions(std::vector<TransactionPtr> txs,
                                ChainState& state,
                                const KeyRing& keys,
                                Summary& summary);

/** Returns a JSON string representing these Transactions
 *  @note pointer counts are not preserved.
 *  @return a JSON string representing these Transactions.
*/
  std::string getJSON() const {
    LOG_DEBUG << "getJSON()";
    MTR_SCOPE_FUNC();
    std::lock_guard<std::mutex> guard(txs_mutex_);
    std::string out("[");
    bool isFirst = true;
    for (auto const& item : txs_) {
      if (isFirst) {
        isFirst = false;
      } else {
        out += ",";
      }
      out += item.second.second->getJSON();
    }
    out += "]";
    return out;
  }

/** Returns a bytestring of these Transactions
 *  @return a bytestring of these Transactions
*/
  std::vector<byte> getCanonicalTransactions() const {
    LOG_DEBUG << "getCanonicalTransactions()";
    MTR_SCOPE_FUNC();
    std::vector<byte> serial;
    std::lock_guard<std::mutex> guard(txs_mutex_);
    for (auto const& item : txs_) {
      std::vector<byte> temp(item.second.second->getCanonical());
      serial.insert(serial.end(), temp.begin(), temp.end());
    }
    return serial;
  }

/** Checks if this pool has pending Transactions
 *  @return true, iff this pool has pending Transactions
 */
  bool hasPendingTransactions() const {
    LOG_DEBUG << "Number pending transactions: "+std::to_string(txs_.size());
    std::lock_guard<std::mutex> guard(txs_mutex_);
    auto empty = txs_.empty();
    return(!empty);
  }

  /**
   *  @return the number of pending Transactions in this pool
   */
  size_t numPendingTransactions() const {
    std::lock_guard<std::mutex> guard(txs_mutex_);
    auto size = txs_.size();
    return(size);
  }

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
                    const DevvContext& context) {
    std::lock_guard<std::mutex> guard(txs_mutex_);
    return proposeBlock_Internal(prev_hash, prior_state, keys, context);
  }

  /**
   * Indicates whether or not this validator has a proposed block
   * thas is awaiting validation.
   * @note uses atomic<bool> indicator
   * @return true, iff this pool has an active ProposedBlock
   */
  bool hasActiveProposal() {
    if (proposal_time_ < GetMillisecondsSinceEpoch()-kPROPOSAL_EXPIRATION_MILLIS) {
      has_active_proposal_ = false;
      pending_proposal_.setNull();
    }
    return(has_active_proposal_);
  }

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
  std::vector<byte> getProposal() {
    LOG_DEBUG << "getProposal()";
    std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
    return pending_proposal_.getCanonical();
  }

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
                        const DevvContext& context) {
    LOG_DEBUG << "reverifyProposal()";
    MTR_SCOPE_FUNC();
    {
      std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
      if (pending_proposal_.isNull()) { return false; }
    }
    if (reverifyTransactions(prior, keys)) {
      std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
      pending_proposal_.setPrevHash(prev_hash);
      return true;
    } else {
      proposeBlock(prev_hash, prior, keys, context);
      return false;
    }
  }

  /**
   *  Checks a remote proposal against this pool's recent transactions
   *  @param proposed - the remote ProposedBlock
   *  @return true, iff no transactions in the remote proposal match
   *                transactions recently finalized in this pool
   *  @return false, a duplicate transaction was detected
   */
  bool isRemoteProposalDuplicateFree(const ProposedBlock& proposed) {
    const std::vector<Signature> sigs = proposed.copyTransactionSignatures();
    for (auto iter = sigs.begin(); iter != sigs.end(); ++iter) {
      if (recent_txs_.find(*iter) != recent_txs_.end()) {
        return false;
	  }
    }
    return true;
  }

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
  bool checkValidation(InputBuffer& buffer, const DevvContext& context) {
    LOG_DEBUG << "checkValidation()";
    std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
    if (pending_proposal_.isNull()) {
      LOG_WARNING << "checkValidation(): pending_proposal_.isNull()";
      return false;
    }
    if (pending_proposal_.checkValidationData(buffer, context)) {
      //provide more time for this proposal since it was just remotely validated
      setProposalTime();
      return true;
	}
	return false;
  }

  /**
   *  Create a new FinalBlock based on this pool's ProposedBlock
   *  @pre ensure this HasProposal() == true before calling this function
   *  @note nullifies this pool's ProposedBlock as a side-effect
   *  @return a FinalBlock based on this pool's ProposedBlock
   */
  const FinalBlock finalizeLocalBlock() {
    LOG_DEBUG << "finalizeLocalBlock()";
    MTR_SCOPE_FUNC();
    std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
    const FinalBlock final_block(finalizeBlock(pending_proposal_));
    pending_proposal_.setNull();
    setHasActiveProposal(false);
    return final_block;
  }

  /**
   *  Create a new FinalBlock based on a remotely serialized FinalBlock
   *  @param serial - a binary representation of the FinalBlock
   *  @param prior - the chain state prior to this new FinalBlock
   *  @param keys - the directory of Addresses and EC keys
   *  @return a FinalBlock based on the remote data provided
   */
  const FinalBlock finalizeRemoteBlock(InputBuffer& buffer,
                                       const ChainState prior,
                                       const KeyRing& keys) {
    LOG_DEBUG << "finalizeRemoteBlock(): prior.size(): " << prior.size();
    MTR_SCOPE_FUNC();
    FinalBlock final(buffer, prior, keys, tcm_);
    removeTransactions(final);
    return final;
  }

  /** Remove unreferenced Transactions from the pool.
   *  @return the number of Transactions removed.
   */
  int GarbageCollect() {
    LOG_DEBUG << "GarbageCollect()";
    //TODO: delete old unrecorded Transactions periodically
    std::vector<Signature> to_erase;
    for (auto iter = recent_txs_.begin(); iter != recent_txs_.end(); ++iter) {
      if (iter->second < GetMillisecondsSinceEpoch()-kDUPLICATE_HORIZON_MILLIS) {
        to_erase.push_back(iter->first);
      }
    }
    for (auto iter = to_erase.begin(); iter != to_erase.end(); ++iter) {
      recent_txs_.erase(*iter);
    }
    //this only cleans the map of potential duplicates
    //the main pool still needs to be garbage collected
    return 0;
  }

  /**
   *  @return the elapsed time since this pool last received a Transaction
   */
  double getElapsedTime() {
    return timer_.elapsed();
  }

 /**
  *  @return the tool to create Transactions in parallel
  */
  TransactionCreationManager& get_transaction_creation_manager() {
    return(tcm_);
  }

 /**
  *  @return the validator mode, which determines the type of Transactions handled.
  */
  eAppMode getMode() const {
    return mode_;
  }

  /**
   * Acquire a lock to ensure permission to propose
   * @return
   */
  std::unique_lock<std::mutex> acquireProposalPermissionLock(bool lock_on_acquire = true) const {
    if (lock_on_acquire){
      return std::unique_lock<std::mutex>(proposal_permission_lock_);
    } else {
      return std::unique_lock<std::mutex>(proposal_permission_lock_, std::defer_lock);
    }
  }

  bool isNewFinalBlockProcessing() const {
    return is_new_final_block_processing_;
  }

  void indicateNewFinalBlock(bool processing_now = true) {
    is_new_final_block_processing_ = processing_now;
  }

 private:
  TxMap txs_;
  std::map<Signature, uint64_t> recent_txs_;

  mutable std::mutex txs_mutex_;

  /// True if this validator has an active proposal currently pending validation
  std::atomic<bool> has_active_proposal_ = ATOMIC_VAR_INIT(false);


  std::atomic<bool> is_new_final_block_processing_ = ATOMIC_VAR_INIT(false);

  mutable std::mutex proposal_permission_lock_;

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
                    const DevvContext& context) {
    LOG_DEBUG << "proposeBlock(): prior_state.size(): " << prior_state.size();
    MTR_SCOPE_FUNC();
    ChainState new_state(prior_state);
    Summary summary = Summary::Create();
    Validation validation = Validation::Create();

    auto validated = collectValidTransactions(prior_state, keys, summary, context);
    if (!validated.empty()) {
      LOG_DEBUG << "Creating new proposal with " << validated.size() << " transactions";
      ProposedBlock new_proposal(prev_hash, validated, summary, validation, new_state, keys);
      size_t node_num = context.get_current_node() % context.get_peer_count();
      new_proposal.signBlock(keys, node_num);
      std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
      LOG_WARNING << "proposeBlock(): canon size: " << new_proposal.getCanonical().size();
      pending_proposal_.shallowCopy(new_proposal);
      setHasActiveProposal(true);
      return true;
    } else {
      LOG_INFO << "collectValidTransactions returned 0 transactions - not proposing";
      pending_proposal_.setNull();
      return false;
    }
  }

  /** Verifies Transactions for this pool.
   *  @note this implementation is greedy in selecting Transactions
   *  @params state the chain state to validate against
   *  @params keys a KeyRing that provides keys for signature verification
   *  @params summary the Summary to update
   *  @return a vector of unrecorded valid transactions
   */
  std::vector<TransactionPtr> collectValidTransactions(const ChainState& state
      , const KeyRing& keys, const Summary& pre_sum, const DevvContext& context) {
    LOG_DEBUG << "CollectValidTransactions(): state.size(): " << state.size();
    std::vector<TransactionPtr> valid;
    MTR_SCOPE_FUNC();
    if (txs_.size() < max_tx_per_block_) {
      LOG_DEBUG << "low incoming transaction volume: sleeping";
      sleep(context.get_max_wait());
    }
    unsigned int num_txs = 0;
    Summary post_sum(Summary::Copy(pre_sum));
    std::map<Address, SmartCoin> aggregate;
    ChainState post_state(ChainState::Copy(state));
    for (auto iter = txs_.begin(); iter != txs_.end(); ++iter) {
      if (recent_txs_.find(iter->second.second->getSignature()) != recent_txs_.end()) {
        LOG_INFO << "CollectValidTransactions: Duplicate transaction in pool.";
        removeTransaction(iter->second.second->getSignature());
        //if valid transactions, propose them
        if (num_txs > 0) break;
        //otherwise, don't need to clean prior to isValidInAggregate()
        //but start the collection procedure over
        return collectValidTransactions(state, keys, pre_sum, context);
      } else if (iter->second.second->isValidInAggregate(post_state, keys,
                                         post_sum, aggregate, state)) {
        valid.push_back(std::move(iter->second.second->clone()));
        iter->second.first++;
        num_txs++;
        if (num_txs >= max_tx_per_block_) { break; }
      } else {
        LOG_INFO << "CollectValidTransactions: Invalid transaction in pool.";
        removeTransaction(iter->second.second->getSignature());
        //if valid transactions, propose them
        if (num_txs > 0) break;
        //if no valid transactions, clean pool, collect again
        //clean
        Summary sum_clone(Summary::Copy(pre_sum));
        ChainState pre_state(ChainState::Copy(state));
        while (!removeInvalidTransactions(pre_state, keys, sum_clone)) {
          //do nothing
        }
        //collect again
        return collectValidTransactions(state, keys, pre_sum, context);
      }
    }
    return valid;
  }

  /** Reverifies Transactions for this pool.
   *  @note this function modifies pending_proposal_
   *  @note if this returns false, a new proposal must be created
   *  @params state the chain state to validate against
   *  @params keys a KeyRing that provides keys for signature verification
   *  @return true iff, all transactions are valid wrt provided chainstate
   *  @return false otherwise, a new proposal must be created
   */
  bool reverifyTransactions(const ChainState& prior, const KeyRing& keys) {
    LOG_DEBUG << "reverifyTransactions()";
    MTR_SCOPE_FUNC();
    std::lock_guard<std::mutex> guard(txs_mutex_);
    Summary summary = Summary::Create();
    std::map<Address, SmartCoin> aggregate;
    ChainState state(prior);
    for (auto const& tx : pending_proposal_.getTransactions()) {
      auto it = txs_.find(tx->getSignature());
      if (it == txs_.end()) {
        //transaction removed from the pool, so pending_proposal is invalid
        return false;
      }
      if (!tx->isValidInAggregate(state, keys, summary, aggregate, prior)) {
        return false;
      }
    }
    return true;
  }

  /** Removes Transaction from this pool
   *  @pre not thread-safe, call from within a mutex guard
   *  @return true if Transaction was removed
   *  @return false otherwise
   */
  bool removeTransaction(const Signature& sig) {
    size_t txs_size = txs_.size();
    if (txs_.erase(sig) == 0) {
      LOG_WARNING << "removeTransaction(): ret = 0, transaction not found: "
                  << sig.getJSON();
    } else {
      LOG_TRACE << "removeTransaction() removes: " << sig.getJSON();
    }
    LOG_DEBUG << "removeTransactions: (size pre/size post) ("
              << txs_size << "/"
              << txs_.size() << ")";
    return true;
  }

  /** Removes invalid transactions from this pool
   *  @pre not thread-safe, call from within a mutex guard
   *  @return true if all invalid transactions in the block were removed
   *  @return false if possible that not all invalid transactions removed
   */
  bool removeInvalidTransactions(const ChainState& state
      , const KeyRing& keys, const Summary& summary) {
    std::map<Address, SmartCoin> aggregate;
    ChainState state_clone(ChainState::Copy(state));
    Summary sum_clone(Summary::Copy(summary));
    for (auto iter = txs_.begin(); iter != txs_.end(); ++iter) {
	  if (!iter->second.second->isValidInAggregate(state_clone, keys, sum_clone
                                                 , aggregate, state)) {
        removeTransaction(iter->second.second->getSignature());
        return false;
      }
    }
    return true;
  }

  /** Removes Transactions in a ProposedBlock from this pool
   *  @param proposed - the ProposedBlock containing Transactions to remove
   *  @pre this should only be called for a ProposedBlock that is finalizing
   *  @pre do not call if there is any chance a PropsedBlock will not finalize
   *  @return true iff, all transactions in the block were removed
   *  @return false otherwise
   */
  bool removeTransactions(const ProposedBlock& proposed) {
    std::lock_guard<std::mutex> guard(txs_mutex_);
    size_t txs_size = txs_.size();
    for (auto const& item : proposed.getTransactions()) {
      if (txs_.erase(item->getSignature()) == 0) {
        LOG_WARNING << "removeTransactions(): ret = 0, transaction not found: "
                    << item->getSignature().getJSON();
      } else {
        recent_txs_.insert(std::pair<Signature, uint64_t>(item->getSignature()
          , GetMillisecondsSinceEpoch()));
        LOG_TRACE << "removeTransactions(): erase returned 1: " << item->getSignature().getJSON();
      }
    }
    LOG_DEBUG << "removeTransactions: (to remove/size pre/size post) ("
              << proposed.getNumTransactions() << "/"
              << txs_size << "/"
              << txs_.size() << ")";
    return true;
  }

  /** Removes Transactions in a FinalBlock from this pool
   *  @param final_block - the FinalBlock containing Transactions to remove
   */
  void removeTransactions(const FinalBlock& final_block) {
    std::lock_guard<std::mutex> guard(txs_mutex_);
    size_t txs_size = txs_.size();
    for (auto const& item : final_block.getTransactions()) {
      txs_.erase(item->getSignature());
      recent_txs_.insert(std::pair<Signature, uint64_t>(item->getSignature()
          , GetMillisecondsSinceEpoch()));
    }
    LOG_DEBUG << "removeTransactions: (size pre/size post) ("
              << txs_size << "/"
              << txs_.size() << ")";
  }

  /** Finalize a ProposedBlock
   *  @param proposed - the ProposedBlock to finalize
   *  @return the FinalBlock based on the provided ProposedBlock
   */
  const FinalBlock finalizeBlock(const ProposedBlock& proposal) {
    LOG_DEBUG << "finalizeBlock()";
    MTR_SCOPE_FUNC();
    removeTransactions(proposal);
    FinalBlock final(proposal);
    return final;
  }

};

} //end namespace Devv
