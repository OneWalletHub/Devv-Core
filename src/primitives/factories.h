/*
 * factories.h provides methods to generate Transactions
 * and/or other primitize Devv types.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "primitives/Tier1Transaction.h"
#include "primitives/Tier2Transaction.h"
#include "common/devv_exceptions.h"

namespace Devv {

static std::unique_ptr<Transaction> CreateTransaction(InputBuffer& buffer,
                                               const KeyRing& keys,
                                               eAppMode mode = eAppMode::T2,
                                               bool calculate_soundness = true) {
  if (mode == eAppMode::T1) {
    auto tx = std::make_unique<Tier1Transaction>(buffer, keys, calculate_soundness);
    return tx;
  } else if (mode == eAppMode::T2) {
    auto tx = Tier2Transaction::CreateUniquePtr(buffer, keys, calculate_soundness);
    return tx;
  } else {
    throw DeserializationError("CreateTransaction(): unknown eAppMode type");
  }
}

} // namespace Devv
