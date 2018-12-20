/*
 * Tier1Transaction.cpp
 * implements methods to generate and copy Tier1Transactions.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "primitives/Tier1Transaction.h"
#include "common/devv_exceptions.h"

namespace Devv {

void Tier1Transaction::Fill(Tier1Transaction& tx,
                    InputBuffer &buffer,
                    const KeyRing &keys,
                    bool calculate_soundness) {
  MTR_SCOPE_FUNC();
  int trace_int = 124;
  MTR_START("Transaction", "Transaction", &trace_int);
  MTR_STEP("Transaction", "Transaction", &trace_int, "step1");

  tx.sum_size_ = buffer.getNextUint64(false);
  size_t tx_size = tx.sum_size_ + kNODE_SIG_BUF_SIZE + uint64Size() + kNODE_ADDR_BUF_SIZE;
  if (buffer.size() < buffer.getOffset() + tx_size) {
    LOG_WARNING << "Invalid serialized T1 transaction, too small!";
    return;
  }

  MTR_STEP("Transaction", "Transaction", &trace_int, "step2");
  buffer.copy(std::back_inserter(tx.canonical_), tx_size);

  MTR_STEP("Transaction", "Transaction", &trace_int, "sound");
  if (calculate_soundness) {
    tx.is_sound_ = tx.isSound(keys);
    if (!tx.is_sound_) {
      LOG_WARNING << "Invalid serialized T1 transaction, not sound!";
    }
  }
  MTR_FINISH("Transaction", "Transaction", &trace_int);
}

Tier1Transaction Tier1Transaction::Create(InputBuffer& buffer,
                                 const KeyRing& keys,
                                 bool calculate_soundness) {
  Tier1Transaction tx;
  Tier1Transaction::Fill(tx, buffer, keys, calculate_soundness);
  return tx;
}

Tier1TransactionPtr Tier1Transaction::CreateUniquePtr(InputBuffer& buffer,
                                 const KeyRing& keys,
                                 bool calculate_soundness) {
  auto tx = std::make_unique<Tier1Transaction>();
  Tier1Transaction::Fill(*tx, buffer, keys, calculate_soundness);
  return tx;
}

} // namespace Devv
