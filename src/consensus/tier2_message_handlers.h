/*
 * tier2_message_handlers.h structure consensus logic for Tier2 validators.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "types/DevvMessage.h"
#include "common/devv_context.h"
#include "common/logger.h"
#include "common/util.h"
#include "consensus/blockchain.h"
#include "consensus/UnrecordedTransactionPool.h"

namespace Devv {

/**
 * Creates a proposal from the UnrecordedTransactionPool
 * @param[in] keys
 * @param[in, out] final_chain
 * @param[in, out] utx_pool
 * @param[in] context
 * @return
 */
std::vector<byte> CreateNextProposal(const KeyRing& keys,
                                           Blockchain& final_chain,
                                           UnrecordedTransactionPool& utx_pool,
                                           const DevvContext& context);
/**
 * Registered with DevvController and called when a eMessageType::FINAL_BLOCK
 * message arrives.
 * @param[in] ptr Pointer to incoming DevvMessage
 * @param[in] context DevvContext
 * @param[in] keys KeyRing keys
 * @param[in, out] final_chain The Blockchain to be updated with the incoming FINAL_BLOCK
 * @param[in, out] utx_pool
 * @param[in] callback Completion callback
 * @return
 */
bool HandleFinalBlock(DevvMessageUniquePtr ptr,
                      const DevvContext& context,
                      const KeyRing& keys,
                      Blockchain& final_chain,
                      UnrecordedTransactionPool& utx_pool,
                      std::function<void(DevvMessageUniquePtr)> callback);

/**
 * Registered with DevvController and called when a eMessageType::PROPOSAL_BLOCK message
 * arrives.
 * @param[in] ptr
 * @param[in] context
 * @param[in] keys
 * @param[in, out] final_chain
 * @param[in, out] tcm
 * @param[in] callback
 * @return
 */
bool HandleProposalBlock(DevvMessageUniquePtr ptr,
                         const DevvContext& context,
                         const KeyRing& keys,
                         const Blockchain& final_chain,
                         TransactionCreationManager& tcm,
                         std::function<void(DevvMessageUniquePtr)> callback);

/**
 *
 * @param ptr
 * @param context
 * @param final_chain
 * @param utx_pool
 * @param callback
 * @return
 */
bool HandleValidationBlock(DevvMessageUniquePtr ptr,
                           const DevvContext& context,
                           Blockchain& final_chain,
                           UnrecordedTransactionPool& utx_pool,
                           std::function<void(DevvMessageUniquePtr)> callback);

/**
 *
 * @param ptr
 * @param final_chain
 * @param context
 * @param keys
 * @param remote_blocks
 * @return
 */
bool HandleBlocksSince(DevvMessageUniquePtr ptr,
                       Blockchain& final_chain,
                       DevvContext context,
                       const KeyRing& keys,
                       const UnrecordedTransactionPool&,
                       uint64_t& remote_blocks);

/**
 *
 * @param ptr
 * @param final_chain
 * @param context
 * @param keys
 * @param callback
 * @return
 */
bool HandleBlocksSinceRequest(DevvMessageUniquePtr ptr,
                              Blockchain& final_chain,
                              const DevvContext& context,
                              const KeyRing& keys,
                              std::function<void(DevvMessageUniquePtr)> callback);

/**
 * Creates a proposal from the UnrecordedTransactionPool
 * @param[in] keys
 * @param[in, out] final_chain
 * @param[in, out] utx_pool
 * @param[in] context
 * @return
 */
void CreateAndSendNextProposal(const KeyRing& keys,
                               Blockchain& final_chain,
                               UnrecordedTransactionPool& utx_pool,
                               const DevvContext& context,
                               DevvMessageCallback callback);

}  // namespace Devv
