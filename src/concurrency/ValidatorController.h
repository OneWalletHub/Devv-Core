/*
 * ValidatorController.h controls consensus worker threads for the Devv protocol.
 *
 * @copywrite  2018 Devvio Inc
 */
#ifndef CONCURRENCY_VALIDATORCONTROLLER_H_
#define CONCURRENCY_VALIDATORCONTROLLER_H_

#include <condition_variable>
#include <mutex>

#include "consensus/KeyRing.h"
#include "consensus/UnrecordedTransactionPool.h"
#include "consensus/blockchain.h"
#include "consensus/chainstate.h"
#include "io/message_service.h"

namespace Devv {

class ValidatorController {
  typedef std::function<std::vector<byte>(const KeyRing &keys,
                                                Blockchain &final_chain,
                                                UnrecordedTransactionPool &utx_pool,
                                                const DevvContext &context)> TransactionAnnouncementCallback;

 public:
  ValidatorController(const KeyRing& keys,
                      DevvContext& context,
                      const ChainState&,
                      Blockchain& final_chain,
                      UnrecordedTransactionPool& utx_pool,
                      eAppMode mode);

  ~ValidatorController();

  void registerOutgoingCallback(DevvMessageCallback callback) {
    outgoing_callback_ = callback;
  }
  /**
   * Process a validator worker message.
   */
  void validatorCallback(DevvMessageUniquePtr ptr);

 private:
  const KeyRing& keys_;
  DevvContext& context_;
  Blockchain& final_chain_;
  UnrecordedTransactionPool& utx_pool_;
  eAppMode mode_;

  mutable std::mutex mutex_;
  //uint64_t remote_blocks_ = 0;

  /// A callback to send the analyzed DevvMessage
  DevvMessageCallback outgoing_callback_;

  TransactionAnnouncementCallback tx_announcement_cb_;
};

} /* namespace Devv */

#endif /* CONCURRENCY_VALIDATORCONTROLLER_H_ */
