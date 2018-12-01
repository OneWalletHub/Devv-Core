/**
 * ConsensusController.cpp controls callbacks for the
 * consensus process between Devv validators.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "ConsensusController.h"
#include "consensus/tier2_message_handlers.h"

namespace Devv {

ConsensusController::ConsensusController(const KeyRing &keys,
                                         DevvContext &context,
                                         const ChainState &,
                                         Blockchain &final_chain,
                                         UnrecordedTransactionPool &utx_pool,
                                         eAppMode mode)
    : keys_(keys)
    , context_(context)
    , final_chain_(final_chain)
    , utx_pool_(utx_pool)
    , mode_(mode)
    , final_block_cb_(HandleFinalBlock)
    , proposal_block_cb_(HandleProposalBlock)
    , validation_block_cb_(HandleValidationBlock)
{
}
void ConsensusController::consensusCallback(DevvMessageUniquePtr ptr) {
  std::lock_guard<std::mutex> guard(mutex_);
  MTR_SCOPE_FUNC();
  try {
    switch (ptr->message_type) {
      case eMessageType::FINAL_BLOCK:LOG_DEBUG << "ConsensusController()::consensusCallback(): FINAL_BLOCK";
        final_block_cb_(std::move(ptr),
                                          context_,
                                          keys_,
                                          final_chain_,
                                          utx_pool_,
                                          [this](DevvMessageUniquePtr p) {
                                            this->outgoing_callback_(std::move(p));
                                          });
        break;

      case eMessageType::PROPOSAL_BLOCK:LOG_DEBUG << "ConsensusController()::consensusCallback(): PROPOSAL_BLOCK";
        utx_pool_.get_transaction_creation_manager().set_keys(&keys_);
        proposal_block_cb_(std::move(ptr),
                                             context_,
                                             keys_,
                                             final_chain_,
                                             utx_pool_,
                                             [this](DevvMessageUniquePtr p) { this->outgoing_callback_(std::move(p)); });
        break;
      case eMessageType::VALID:LOG_DEBUG << "ConsensusController()::consensusCallback(): VALIDATION";
        validation_block_cb_(std::move(ptr),
                                               context_,
                                               final_chain_,
                                               utx_pool_,
                                               [this](DevvMessageUniquePtr p) {
                                                 this->outgoing_callback_(std::move(p));
                                               });
        break;
      default:
        throw DevvMessageError("consensusCallback(): Unexpected message type:"
                                      + std::to_string(ptr->message_type));
    }
  } catch (const std::exception &e) {
    LOG_FATAL << FormatException(&e, "ValidatorController.consensusCallback()");
    throw e;
  }
}

} // namespace Devv
