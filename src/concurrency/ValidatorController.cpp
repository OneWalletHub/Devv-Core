/*
 * ValidatorController.cpp controls consensus worker threads for the Devv protocol.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "ValidatorController.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <string>
#include <ctime>
#include <boost/filesystem.hpp>

#include "common/devv_exceptions.h"
#include "common/logger.h"
#include "io/message_service.h"
#include "consensus/KeyRing.h"
#include "primitives/Tier1Transaction.h"
#include "primitives/Tier2Transaction.h"
#include "consensus/tier2_message_handlers.h"

typedef std::chrono::milliseconds millisecs;

namespace fs = boost::filesystem;

namespace Devv {

#define DEBUG_TRANSACTION_INDEX \
  (processed + (context_.get_current_node()+1)*11000000)

ValidatorController::ValidatorController(
    const KeyRing& keys,
    DevvContext& context,
    const ChainState&,
    Blockchain& final_chain,
    UnrecordedTransactionPool& utx_pool,
    eAppMode mode)
    : keys_(keys)
    , context_(context)
    , final_chain_(final_chain)
    , utx_pool_(utx_pool)
    , mode_(mode)
    , tx_announcement_cb_(CreateNextProposal)
{
  LOG_DEBUG << "ValidatorController created - mode " << mode_;
}

ValidatorController::~ValidatorController() {
  LOG_DEBUG << "~ValidatorController()";
}

void ValidatorController::validatorCallback(DevvMessageUniquePtr ptr) {
  LOG_DEBUG << "ValidatorController::validatorCallback()";
  //Do not remove lock_guard, function may use atomic<bool> as concurrency signal
  std::lock_guard<std::mutex> guard(mutex_);
  if (ptr == nullptr) {
    throw DevvMessageError("validatorCallback(): ptr == nullptr, ignoring");
  }
  LogDevvMessageSummary(*ptr, "validatorCallback");

  LOG_DEBUG << "ValidatorController::validatorCallback()";
  MTR_SCOPE_FUNC();

  if (ptr->message_type != TRANSACTION_ANNOUNCEMENT) {
    throw DevvMessageError("Wrong message type arrived: " + std::to_string(ptr->message_type));
  }

  utx_pool_.addTransactions(ptr->data, keys_);
  size_t block_height = final_chain_.size();
  LOG_DEBUG << "current_node(" << context_.get_current_node() << ")" \
              <<" peer_count(" << context_.get_peer_count() << ")" \
              << " block_height (" << block_height << ")";

  if (block_height%context_.get_peer_count() != context_.get_current_node()%context_.get_peer_count()) {
    LOG_INFO << "NOT PROPOSING! (" << block_height % context_.get_peer_count() << ")" <<
             " (" << context_.get_current_node() % context_.get_peer_count() << ")";
    return;
  }

  // Acquire the proposal lock, but don't lock it yet (we don't want to block
  // on a lock here)
  auto proposal_lock = utx_pool_.acquireProposalPermissionLock(false);

  // Try to acquire the lock, break out if the lock is in use
  if (!proposal_lock.try_lock()) {
    LOG_INFO << "validatorCallback(): proposal_lock.try_lock() == false, not proposing";
    return;
  }
  LOG_DEBUG << "validatorCallback(): proposal_lock acquired";

  // Do not propose if proposal is forestalled by HandleFinalBlock
  if (utx_pool_.isNewFinalBlockProcessing()) {
    LOG_INFO << "utx_pool_.isProposalForestalled() == true, not proposing";
    return;
  }

  CreateAndSendNextProposal(keys_,
                            final_chain_,
                            utx_pool_,
                            context_,
                            outgoing_callback_
  );
  LOG_DEBUG << "ValidatorController::validatorCallback(): proposal message sent";
}

} //end namespace Devv
