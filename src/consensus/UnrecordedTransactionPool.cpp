/*
 * UnrecordedTransactionPool.h
 * tracks Transactions that have not yet been encoded in the blockchain.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "consensus/UnrecordedTransactionPool.h"

namespace Devv {

bool UnrecordedTransactionPool::addAndVerifyTransactions(std::vector<TransactionPtr> txs,
                                                         ChainState& state,
                                                         const KeyRing& keys,
                                                         Summary& summary) {
  LOG_DEBUG << "addAndVerifyTransactions()";
  MTR_SCOPE_FUNC();
  std::lock_guard<std::mutex> guard(txs_mutex_);
  for (TransactionPtr& item : txs) {
    Signature sig = item->getSignature();
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

} // namespace Devv
