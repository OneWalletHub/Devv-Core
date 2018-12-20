/*
 * FinalBlock.cpp
 * Implements tools to Create proposed blocks
 *
 * @copywrite  2018 Devvio Inc
 */
#include "ProposedBlock.h"
#include "primitives/json_interface.h"

namespace Devv {

bool ProposedBlock::validate(const KeyRing& keys) const {
  LOG_DEBUG << "validate()";
  MTR_SCOPE_FUNC();
  if (transaction_vector_.size() < 1) {
    LOG_WARNING << "Trying to validate empty block.";
    return false;
  }

  if (!summary_.isSane()) {
    LOG_WARNING << "Summary is invalid in block.validate()!\n";
    LOG_DEBUG << "Summary state: " + GetJSON(summary_);
    return false;
  }

  std::vector<byte> md = summary_.getCanonical();
  for (auto& sig : vals_.getValidationMap()) {
    EC_KEY* eckey = keys.getKey(sig.first);
    auto verified = VerifyByteSig(eckey, DevvHash(md), sig.second);
    EC_KEY_free(eckey);
    if (!verified) {
      LOG_WARNING << "Invalid block signature";
      LOG_DEBUG << "Block state: " + GetJSON(*this);
      LOG_DEBUG << "Block Node Addr: " + sig.first.getJSON();
      LOG_DEBUG << "Block Node Sig: " + sig.second.getJSON();
      return false;
    }
  }

  return true;
}

} // namespace Devv
