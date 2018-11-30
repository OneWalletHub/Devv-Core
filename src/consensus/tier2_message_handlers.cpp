/*
 * tier2_message_handlers.cpp implement consensus logic for Tier2 validators.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "consensus/tier2_message_handlers.h"
#include "primitives/buffers.h"

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace Devv {

std::vector<byte> CreateNextProposal(const KeyRing& keys,
                        Blockchain& final_chain,
                        UnrecordedTransactionPool& utx_pool,
                        const DevvContext& context) {
  MTR_SCOPE_FUNC();
  size_t block_height = final_chain.size();

  if (!(block_height % 100) || !((block_height + 1) % 100)) {
    LOG_NOTICE << "Processing @ final_chain_.size: (" << std::to_string(block_height) << ")";
  }

  if (!utx_pool.HasProposal() && utx_pool.hasPendingTransactions()) {
    LOG_DEBUG << "block_height: " << block_height;
    bool result = true;
    if (block_height > 0) {
      Hash prev_hash = DevvHash(final_chain.back()->getCanonical());
      ChainState prior = final_chain.getHighestChainState();
      LOG_DEBUG << "ChainState prior.size(): " << prior.size();
      result = utx_pool.proposeBlock(prev_hash, prior, keys, context);
    } else {
      Hash prev_hash = DevvHash({'G', 'e', 'n', 'e', 's', 'i', 's'});
      ChainState prior;
      result = utx_pool.proposeBlock(prev_hash, prior, keys, context);
    }
    if (!result) {
      throw std::runtime_error("proposeBlock returned false, not proposing");
    }
  } else {
    std::string err = "Creating a proposal with no pending transactions!!";
    LOG_ERROR << err;
    throw std::runtime_error(err);
  }

  LOG_INFO << "Proposal #"+std::to_string(block_height+1)+".";

  return utx_pool.getProposal();
}

bool HandleFinalBlock(DevvMessageUniquePtr ptr,
                      const DevvContext& context,
                      const KeyRing& keys,
                      Blockchain& final_chain,
                      UnrecordedTransactionPool& utx_pool,
                      std::function<void(DevvMessageUniquePtr)> callback) {

  if (ptr->message_type != eMessageType::FINAL_BLOCK) {
    throw std::runtime_error("HandleFinalBlock: message != eMessageType::FINAL_BLOCK");
  }

  MTR_SCOPE_FUNC();
  //Make the incoming block final
  //if pending proposal, makes sure it is still valid
  //if no pending proposal, check if should make one

  InputBuffer buffer(ptr->data);
  LogDevvMessageSummary(*ptr, "HandleFinalBlock()");

  ChainState prior = final_chain.getHighestChainState();
  LOG_DEBUG << "prior.size(): " << prior.size();
  utx_pool.LockProposals();
  FinalPtr top_block = std::make_shared<FinalBlock>(utx_pool.FinalizeRemoteBlock(
                                               buffer, prior, keys));
  final_chain.push_back(top_block);
  LOG_NOTICE << "final_chain.push_back(): Estimated rate: (ntxs/duration): rate -> "
             << "(" << final_chain.getNumTransactions() << "/"
             << utx_pool.getElapsedTime() << "): "
             << final_chain.getNumTransactions() / (utx_pool.getElapsedTime()/1000) << " txs/sec";

  // Did we send a message
  bool sent_message = false;

  if (utx_pool.HasProposal()) {
    LOG_DEBUG << "HandleFinalBlock: utx_pool.HasProposal: " << utx_pool.HasProposal();
    ChainState current = top_block->getChainState();
    Hash prev_hash = DevvHash(top_block->getCanonical());
    utx_pool.ReverifyProposal(prev_hash, current, keys, context);
  } else {
    LOG_DEBUG << "HandleFinalBlock: utx_pool.HasProposal: " << utx_pool.HasProposal();
  }

  size_t block_height = final_chain.size();

  if (!utx_pool.hasPendingTransactions()) {
    LOG_INFO << "All pending transactions processed.";
    utx_pool.UnlockProposals();
  } else if (block_height % context.get_peer_count() == context.get_current_node() % context.get_peer_count()) {
    if (!utx_pool.HasProposal()) {
      std::vector<byte> proposal;
      try {
        proposal = CreateNextProposal(keys, final_chain, utx_pool, context);
      } catch (std::runtime_error err) {
        LOG_INFO << "NOT PROPOSING: " << err.what();
        utx_pool.UnlockProposals();
        return false;
      }
      if (!ProposedBlock::isNullProposal(proposal)) {
        // Create message
        auto propose_msg =
            std::make_unique<DevvMessage>(context.get_shard_uri(),
                                             PROPOSAL_BLOCK,
                                             proposal,
                                             ((block_height + 1) + (context.get_current_node() + 1) * 1000000));
        // FIXME (spm): define index value somewhere
        LogDevvMessageSummary(*propose_msg, "CreateNextProposal");
        callback(std::move(propose_msg));
        sent_message = true;
      } else {
        LOG_WARNING << "HandleFinalBlock: proposal is null ;; ProposedBlock::isNullProposal(proposal) == NULL";
      }
    } else {
      LOG_DEBUG << "HandleFinalBlock: Sent proposal, utx_pool has no more proposals";
    }
  } else {
    LOG_DEBUG << "if (block_height % context.get_peer_count() == context.get_current_node() % context.get_peer_count())";
    LOG_DEBUG << "Pending transactions but not my turn: " <<
              "("<< block_height<<"%"<< context.get_peer_count()<< ") != " <<
              "(" << context.get_current_node() << "%" << context.get_peer_count() << ") " <<
              ": (" << block_height % context.get_peer_count() << " != " <<
              context.get_current_node() % context.get_peer_count() <<
              ") pending: " << utx_pool.hasPendingTransactions();
  }
  return sent_message;
}

bool HandleProposalBlock(DevvMessageUniquePtr ptr,
                         const DevvContext& context,
                         const KeyRing& keys,
                         const Blockchain& final_chain,
                         TransactionCreationManager& tcm,
                         std::function<void(DevvMessageUniquePtr)> callback) {

  if (ptr->message_type != eMessageType::PROPOSAL_BLOCK) {
    throw std::runtime_error("HandleProposalBlock: message != eMessageType::PROPOSAL_BLOCK");
  }

  MTR_SCOPE_FUNC();

  LogDevvMessageSummary(*ptr, "HandleProposalBlock() -> Incoming");

  ChainState prior = final_chain.getHighestChainState();
  InputBuffer buffer(ptr->data);
  ProposedBlock to_validate(ProposedBlock::Create(buffer, prior, keys, tcm));

  if (!to_validate.validate(keys)) {
    LOG_WARNING << "ProposedBlock is invalid!";
    return false;
  }
  size_t node_num = context.get_current_node() % context.get_peer_count();
  if (!to_validate.signBlock(keys, node_num)) {
    LOG_WARNING << "ProposedBlock.signBlock failed!";
    return false;
  }
  LOG_DEBUG << "Proposed block is valid.";
  LOG_DEBUG << "Proposed ChainState size: " << to_validate.getBlockState().size();

  std::vector<byte> validation(to_validate.getValidationData());

  auto valid = std::make_unique<DevvMessage>(context.get_shard_uri(),
                                                VALID,
                                                validation,
                                                ptr->index);
  LogDevvMessageSummary(*valid, "HandleProposalBlock() -> Validation");
  callback(std::move(valid));
  return true;
}

bool HandleValidationBlock(DevvMessageUniquePtr ptr,
                           const DevvContext& context,
                           Blockchain& final_chain,
                           UnrecordedTransactionPool& utx_pool,
                           std::function<void(DevvMessageUniquePtr)> callback) {

  if (ptr->message_type != eMessageType::VALID) {
    throw std::runtime_error("HandleValidationBlock: message != eMessageType::VALID");
  }

  MTR_SCOPE_FUNC();
  bool sent_message = false;
  InputBuffer buffer(ptr->data);
  LogDevvMessageSummary(*ptr, "HandleValidationBlock() -> Incoming");

  if (utx_pool.CheckValidation(buffer, context)) {
    //block can be finalized, so finalize
    LOG_DEBUG << "Ready to finalize block.";
    FinalPtr top_block = std::make_shared<FinalBlock>(utx_pool.FinalizeLocalBlock());
    final_chain.push_back(top_block);
    LOG_NOTICE << "final_chain.push_back(): Estimated rate: (ntxs/duration): rate -> "
               << "(" << final_chain.getNumTransactions() << "/"
               << utx_pool.getElapsedTime() << "): "
               << final_chain.getNumTransactions() / (utx_pool.getElapsedTime()/1000) << " txs/sec";

    std::vector<byte> final_msg = top_block->getCanonical();

    auto final_block = std::make_unique<DevvMessage>(context.get_shard_uri(), FINAL_BLOCK, final_msg, ptr->index);
    LogDevvMessageSummary(*final_block, "HandleValidationBlock() -> Final block");
    callback(std::move(final_block));
    sent_message = true;
  }

  return sent_message;
}

bool HandleBlocksSinceRequest(DevvMessageUniquePtr ptr,
                              Blockchain& final_chain,
                              const DevvContext& context,
                              const KeyRing& keys,
                              std::function<void(DevvMessageUniquePtr)> callback) {
  LogDevvMessageSummary(*ptr, "HandleBlocksSinceRequest() -> Incoming");
  if (ptr->data.size() < 16) {
    LOG_WARNING << "BlockSinceRequest is too small!";
    return false;
  }

  uint64_t height = BinToUint32(ptr->data, 0);
  uint64_t node = BinToUint32(ptr->data, 8);
  std::vector<byte> raw = final_chain.PartialBinaryDump(height);
  LOG_INFO << "HandleBlocksSinceRequest(): height(" << height << "), node(" << node << ")";
  /*
  if (final_chain.size() < 2) {
    LOG_WARNING << "HandleBlocksSinceRequest() -> final_chain.size() < 2, no blocks to send";
    return false;
  }
  if (final_chain.size() <= height) {
    LOG_WARNING << "HandleBlocksSinceRequest() -> final_chain.size() <= height ("
                << final_chain.size() << " <= " << height << "), no blocks to send";
    return false;
  }
  */

  InputBuffer buffer(raw);
  if (context.get_app_mode() == eAppMode::T2) {
    std::vector<byte> tier1_data;
    ChainState temp;
    while (buffer.getOffset() < buffer.size()) {
      LOG_DEBUG << "HandleBlocksSinceRequest(): offset/raw.size() ("
                << buffer.getOffset() << "/" << buffer.size() << ")";
        //constructor increments offset by reference
      FinalBlock one_block(FinalBlock::Create(buffer, temp));
      Summary sum = Summary::Copy(one_block.getSummary());
      Validation val(one_block.getValidation());
      std::pair<Address, Signature> pair(val.getFirstValidation());
      Tier1Transaction tx(sum, pair.second, pair.first, keys);
      std::vector<byte> tx_canon(tx.getCanonical());
      tier1_data.insert(tier1_data.end(), tx_canon.begin(), tx_canon.end());
    }
    auto response = std::make_unique<DevvMessage>(context.get_uri_from_index(node),
                                                     TRANSACTION_ANNOUNCEMENT,
                                                     tier1_data,
                                                     ptr->index);
    callback(std::move(response));
    return true;
  } else if (context.get_app_mode() == eAppMode::T1) {
    uint64_t covered_height = final_chain.size()-1;
    std::vector<byte> bin_height;
    Uint64ToBin(covered_height, bin_height);
    //put height at beginning of message
    raw.insert(raw.begin(), bin_height.begin(), bin_height.end());
    auto response = std::make_unique<DevvMessage>(context.get_uri_from_index(node),
                                                     BLOCKS_SINCE,
                                                     raw,
                                                     ptr->index);
    callback(std::move(response));
    return true;
  } else {
    LOG_WARNING << "Unsupported mode: " << context.get_app_mode();
  }
  return false;
}

bool HandleBlocksSince(DevvMessageUniquePtr ptr,
                              Blockchain& final_chain,
                              DevvContext context,
                              const KeyRing& keys,
                              const UnrecordedTransactionPool&,
                              uint64_t& remote_blocks) {
  LogDevvMessageSummary(*ptr, "HandleBlocksSince() -> Incoming");

  InputBuffer buffer(ptr->data);
  if (buffer.size() < 8) {
    LOG_WARNING << "BlockSince is too small!";
    return false;
  }
  uint64_t height = buffer.getNextUint64();

  if (context.get_app_mode() == eAppMode::T2) {
    std::vector<Address> wallets = keys.getDesignatedWallets(context.get_current_shard());
    ChainState state = final_chain.getHighestChainState();
    while (buffer.getOffset() < buffer.size()) {
      //constructor increments offset by reference
      FinalBlock one_block(FinalBlock::Create(buffer, state));
      uint64_t elapsed = GetMillisecondsSinceEpoch() - one_block.getBlockTime();
      Summary sum = Summary::Copy(one_block.getSummary());
      for (auto const& addr : wallets) {
        std::vector<SmartCoin> coins = sum.getCoinsByAddr(addr, elapsed);
        for (auto const& coin : coins) {
          state.addCoin(coin);
        }
      }
      //TODO: update upcoming state in utx pool
    }
    if (height > remote_blocks) { remote_blocks = height; }
    LOG_INFO << "Finished updating local state for Tier1 block height: "+std::to_string(height);
  }
  return false;
}

}  // namespace Devv
