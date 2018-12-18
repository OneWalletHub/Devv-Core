/*
 * UnrecordedTransactionPool.h
 * tracks Transactions that have not yet been encoded in the blockchain.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "consensus/UnrecordedTransactionPool.h"

#include "common/logger.h"

namespace Devv {

UnrecordedTransactionPool::UnrecordedTransactionPool(const ChainState& prior, eAppMode mode, size_t max_tx_per_block)
    : txs_()
    , pending_proposal_(ProposedBlock::Create(prior))
    , max_tx_per_block_(max_tx_per_block)
    , tcm_(mode)
    , mode_(mode)
{
  LOG_DEBUG << "UnrecordedTransactionPool(const ChainState& prior)";
}

bool UnrecordedTransactionPool::addTransactions(const std::vector<byte>& serial, const KeyRing& keys) {
  LOG_DEBUG << "addTransactions(const std::vector<byte>& serial, const KeyRing& keys)";
  MTR_SCOPE_FUNC();
  std::vector<TransactionPtr> temp;
  InputBuffer buffer(serial);
  while (buffer.getOffset() < buffer.size()) {
    auto tx = CreateTransaction(buffer, keys, mode_);
    if (recent_txs_.find(tx->getSignature()) == recent_txs_.end()) {
      temp.push_back(std::move(tx));
    } else {
      LOG_DEBUG << "Avoided adding duplicate tx.";
    }
  }
  return addTransactions(temp, keys);
}

bool UnrecordedTransactionPool::addTransactions(std::vector<TransactionPtr>& txs, const KeyRing& keys) {
  LOG_DEBUG << "addTransactions(std::vector<Transaction> txs, const KeyRing& keys)";
  MTR_SCOPE_FUNC();
  std::lock_guard<std::mutex> guard(txs_mutex_);
  bool all_good = true;
  CASH_TRY {
    int counter = 0;
    for (TransactionPtr& item : txs) {
      Signature sig = item->getSignature();
      if (recent_txs_.find(sig) != recent_txs_.end()) {
        LOG_DEBUG << "Avoided adding duplicate tx.";
        continue;
      }
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

bool UnrecordedTransactionPool::addAndVerifyTransactions(std::vector<TransactionPtr> txs,
                                                         ChainState& state,
                                                         const KeyRing& keys,
                                                         Summary& summary) {
  LOG_DEBUG << "addAndVerifyTransactions()";
  MTR_SCOPE_FUNC();
  std::lock_guard<std::mutex> guard(txs_mutex_);
  for (TransactionPtr& item : txs) {
    Signature sig = item->getSignature();
    if (recent_txs_.find(sig) != recent_txs_.end()) {
      LOG_DEBUG << "Avoided adding duplicate tx.";
      continue;
    }
    auto it = txs_.find(sig);
    bool valid = it->second.second->isValid(state, keys, summary);
    if (!valid) { return false; } //tx is invalid
    if (it != txs_.end()) {
      it->second.first++;
    } else if (item->isSound(keys)) {
      SharedTransaction pair((uint8_t) 0, std::move(item));
      pair.first++;
      txs_.insert(std::pair<Signature, SharedTransaction>(sig, std::move(pair)));
      if (num_cum_txs_ == 0) {
        LOG_NOTICE << "addTransactions(): First transaction added to transaction map";
        timer_.reset();
#ifdef MTR_ENABLED
        trace_ = std::make_unique<MTRScopedTrace>("timer", "lifetime2");
#endif
      }
      num_cum_txs_++;

    } else { //tx is unsound
      return false;
    }
  }
  return true;
}

std::string UnrecordedTransactionPool::getJSON() const {
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

std::vector<byte> UnrecordedTransactionPool::getCanonicalTransactions() const {
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

bool UnrecordedTransactionPool::hasPendingTransactions() const {
  LOG_DEBUG << "Number pending transactions: "+std::to_string(txs_.size());
  std::lock_guard<std::mutex> guard(txs_mutex_);
  auto empty = txs_.empty();
  return(!empty);
}

size_t UnrecordedTransactionPool::numPendingTransactions() const {
  std::lock_guard<std::mutex> guard(txs_mutex_);
  auto size = txs_.size();
  return(size);
}

bool UnrecordedTransactionPool::proposeBlock(const Hash& prev_hash,
                                             const ChainState& prior_state,
                                             const KeyRing& keys,
                                             const DevvContext& context) {
  std::lock_guard<std::mutex> guard(txs_mutex_);
  return proposeBlock_Internal(prev_hash, prior_state, keys, context);
}

bool UnrecordedTransactionPool::hasActiveProposal() {
  if (proposal_time_ < GetMillisecondsSinceEpoch()-kPROPOSAL_EXPIRATION_MILLIS) {
    has_active_proposal_ = false;
    pending_proposal_.setNull();
  }
  return(has_active_proposal_);
}

std::vector<byte> UnrecordedTransactionPool::getProposal() {
  LOG_DEBUG << "getProposal()";
  std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
  return pending_proposal_.getCanonical();
}

bool UnrecordedTransactionPool::reverifyProposal(const Hash& prev_hash,
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

bool UnrecordedTransactionPool::isRemoteProposalDuplicateFree(const ProposedBlock& proposed) {
  const std::vector<Signature> sigs = proposed.copyTransactionSignatures();
  for (auto iter = sigs.begin(); iter != sigs.end(); ++iter) {
    if (recent_txs_.find(*iter) != recent_txs_.end()) {
      return false;
    }
    LOG_INFO << "VERIFIED_PROPOSED_TX: " << iter->getJSON();
  }
  return true;
}

bool UnrecordedTransactionPool::checkValidation(InputBuffer& buffer, const DevvContext& context) {
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

const FinalBlock UnrecordedTransactionPool::finalizeLocalBlock() {
  LOG_DEBUG << "finalizeLocalBlock()";
  MTR_SCOPE_FUNC();
  std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
  const FinalBlock final_block(finalizeBlock(pending_proposal_));
  pending_proposal_.setNull();
  setHasActiveProposal(false);
  return final_block;
}

const FinalBlock UnrecordedTransactionPool::finalizeRemoteBlock(InputBuffer& buffer,
                                                                const ChainState prior,
                                                                const KeyRing& keys) {
  LOG_DEBUG << "finalizeRemoteBlock(): prior.size(): " << prior.size();
  MTR_SCOPE_FUNC();
  FinalBlock final(buffer, prior, keys, tcm_);
  removeTransactions(final);
  return final;
}

int UnrecordedTransactionPool::GarbageCollect() {
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

bool UnrecordedTransactionPool::proposeBlock_Internal(const Hash& prev_hash,
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

std::vector<TransactionPtr> UnrecordedTransactionPool::collectValidTransactions(const ChainState& state,
                                                                                const KeyRing& keys,
                                                                                const Summary& pre_sum,
                                                                                const DevvContext& context) {
  LOG_DEBUG << "collectValidTransactions(): state.size(): " << state.size();
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
    auto sig = iter->second.second->getSignature().getJSON();
    if (recent_txs_.find(iter->second.second->getSignature()) != recent_txs_.end()) {
      LOG_INFO << "collectValidTransactions(): Duplicate transaction in pool blk("<<state.size()<<"): " << sig;
      removeTransaction(iter->second.second->getSignature());
      //if valid transactions, propose them
      if (num_txs > 0) {
        LOG_INFO << "BREAKing out of collectValidTransactions: " << num_txs;
        break;
      }
      //otherwise, don't need to clean prior to isValidInAggregate()
      //but start the collection procedure over
      return collectValidTransactions(state, keys, pre_sum, context);
    } else if (iter->second.second->isValidInAggregate(post_state, keys,
                                                       post_sum, aggregate, state)) {
      LOG_DEBUG << "VALIDTX blk("<<state.size()<<"): " << iter->second.second->getSignature().getJSON();
      valid.push_back(std::move(iter->second.second->clone()));
      iter->second.first++;
      num_txs++;
      if (num_txs >= max_tx_per_block_) { break; }
    } else {
      LOG_INFO << "CollectValidTransactions: Invalid transaction in pool blk("<<state.size()<<"): " << sig;
      removeTransaction(iter->second.second->getSignature());
      //if valid transactions, propose them
      if (num_txs > 0) {
        LOG_INFO << "BREAKing out of collectValidTransactions: " << num_txs;
        break;
      }
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

bool UnrecordedTransactionPool::reverifyTransactions(const ChainState& prior, const KeyRing& keys) {
  LOG_DEBUG << "reverifyTransactions()";
  MTR_SCOPE_FUNC();
  std::lock_guard<std::mutex> guard(txs_mutex_);
  Summary summary = Summary::Create();
  std::map<Address, SmartCoin> aggregate;
  ChainState state(prior);
  std::lock_guard<std::mutex> proposal_guard(pending_proposal_mutex_);
  for (auto const& tx : pending_proposal_.getTransactions()) {
    auto it = txs_.find(tx->getSignature());
    if (it == txs_.end()) {
      //transaction removed from the pool, so pending_proposal is invalid
      return false;
    }
    if (recent_txs_.find(tx->getSignature()) != recent_txs_.end()) {
      LOG_DEBUG << "Duplicate transaction detected, remove and re-propose.";
      removeTransaction(tx->getSignature());
      return false;
    }
    if (!tx->isValidInAggregate(state, keys, summary, aggregate, prior)) {
      return false;
    }
    LOG_DEBUG << "REVERIFIED: transaction blk("<<state.size()<<"): " << tx->getSignature().getJSON();
  }
  return true;
}

bool UnrecordedTransactionPool::removeTransaction(const Signature& sig) {
  size_t txs_size = txs_.size();
  if (txs_.erase(sig) == 0) {
    LOG_WARNING << "removeTransaction(): ret = 0, transaction not found: "
                << sig.getJSON();
  } else {
    LOG_DEBUG << "removeTransaction() removes: " << sig.getJSON();
  }
  LOG_DEBUG << "removeTransactions: (size pre/size post) ("
            << txs_size << "/"
            << txs_.size() << ")";
  return true;
}

bool UnrecordedTransactionPool::removeInvalidTransactions(const ChainState& state,
                                                          const KeyRing& keys,
                                                          const Summary& summary) {
  std::map<Address, SmartCoin> aggregate;
  ChainState state_clone(ChainState::Copy(state));
  Summary sum_clone(Summary::Copy(summary));
  for (auto iter = txs_.begin(); iter != txs_.end(); ++iter) {
    if (!iter->second.second->isValidInAggregate(state_clone, keys, sum_clone
        , aggregate, state)) {
      removeTransaction(iter->second.second->getSignature());
      return false;
    }
    LOG_DEBUG << "KEEP TX blk("<<state.size()<<"): " << iter->second.second->getSignature().getJSON();
  }
  return true;
}

bool UnrecordedTransactionPool::removeTransactions(const ProposedBlock& proposed) {
  std::lock_guard<std::mutex> guard(txs_mutex_);
  size_t txs_size = txs_.size();
  for (auto const& item : proposed.getTransactions()) {
    if (txs_.erase(item->getSignature()) == 0) {
      LOG_WARNING << "removeTransactions(): ret = 0, transaction not found: "
                  << item->getSignature().getJSON();
    } else {
      LOG_DEBUG << "removeTransactions(): erase returned 1: " << item->getSignature().getJSON();
    }
    recent_txs_.insert(std::pair<Signature, uint64_t>(item->getSignature()
        , GetMillisecondsSinceEpoch()));
  }
  LOG_DEBUG << "removeTransactions: (to remove/size pre/size post) ("
            << proposed.getNumTransactions() << "/"
            << txs_size << "/"
            << txs_.size() << ")";
  return true;
}

void UnrecordedTransactionPool::removeTransactions(const FinalBlock& final_block) {
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

const FinalBlock UnrecordedTransactionPool::finalizeBlock(const ProposedBlock& proposal) {
  LOG_DEBUG << "finalizeBlock()";
  MTR_SCOPE_FUNC();
  removeTransactions(proposal);
  FinalBlock final(proposal);
  return final;
}

} // namespace Devv
