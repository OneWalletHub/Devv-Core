/*
 * BlockchainModule.cpp handles orchestration of modules, startup, shutdown,
 * and response to signals.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "modules/BlockchainModule.h"

#include <atomic>
#include <functional>
#include <string>
#include <iostream>
#include <memory>
#include <csignal>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/crypto.h>
#include <boost/thread/thread.hpp>

#include "consensus/chainstate.h"
#include "consensus/tier2_message_handlers.h"
#include "io/zhelpers.hpp"
#include "oracles/api.h"
#include "oracles/data.h"
#include "oracles/dcash.h"
#include "oracles/devvprotect.h"
#include "oracles/dneroavailable.h"
#include "oracles/dnerowallet.h"
#include "oracles/id.h"
#include "oracles/revert.h"
#include "oracles/vote.h"
#include "primitives/Transaction.h"
#include "types/DevvMessage.h"
#include "common/devv_exceptions.h"

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace Devv
{

std::atomic<bool> request_shutdown(false); /** has a shutdown been requested? */
bool isCryptoInit = false;

std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

bool InitCrypto()
{
  try {
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    isCryptoInit = true;
    return true;
  } catch (const std::exception& e) {
    LOG_FATAL << FormatException(&e, "BlockchainModule.InitCrypto");
  }
  return(false);
}

BlockchainModule::BlockchainModule(_constructor_tag,
                                   io::TransactionServer& server,
                                   io::TransactionClient& client,
                                   io::TransactionClient& loopback_client,
                                   const KeyRing& keys,
                                   const ChainState &prior,
                                   eAppMode mode,
                                   DevvContext &context,
                                   size_t max_tx_per_block)
    : server_(server),
      client_(client),
      loopback_client_(loopback_client),
      keys_(keys),
      prior_(prior),
      mode_(mode),
      app_context_(context),
      final_chain_("final_chain_"),
      utx_pool_(prior, mode, max_tx_per_block),
      consensus_controller_(keys_, app_context_, prior_, final_chain_, utx_pool_, mode_),
      internetwork_controller_(keys_, app_context_, prior_, final_chain_, utx_pool_, mode_),
      validator_controller_(keys_, app_context_, prior_, final_chain_, utx_pool_, mode_)
{
  LOG_INFO << "Hello from node: " << app_context_.get_uri() << "!!";
}

std::unique_ptr<BlockchainModule> BlockchainModule::Create(io::TransactionServer &server,
                                        io::TransactionClient &client,
                                        io::TransactionClient &loopback_client,
                                        const KeyRing &keys,
                                        const ChainState &prior,
                                        eAppMode mode,
                                        DevvContext &context,
                                        size_t max_tx_per_block) {

  /// Create the ValidatorModule which holds all of the controllers
  auto blockchain_module_ptr = std::make_unique<BlockchainModule>(_constructor_tag{},
                                                                  server,
                                                                  client,
                                                                  loopback_client,
                                                                  keys,
                                                                  prior,
                                                                  mode,
                                                                  context,
                                                                  max_tx_per_block);

  /// Register the outgoing callback to send over zmq
  auto outgoing_callback =
      [&server](DevvMessageUniquePtr p) { server.queueMessage(std::move(p)); };

  /// Shorten the name of the controllers
  auto& vc = blockchain_module_ptr->validator_controller_;
  auto& cc = blockchain_module_ptr->consensus_controller_;
  auto& ic = blockchain_module_ptr->internetwork_controller_;

  vc.registerOutgoingCallback(outgoing_callback);
  /// The controllers contain the the algorithms, the ParallelExecutor parallelizes them

  blockchain_module_ptr->validator_executor_ =
      std::make_unique<ParallelExecutor<ValidatorController>>(vc, 1);
  /// Attach a callback to be run in the threads

  blockchain_module_ptr->validator_executor_->attachCallback(
      [&](DevvMessageUniquePtr p) { vc.validatorCallback(std::move(p));
  });


  /// Register the outgoing callback to send over zmq
  cc.registerOutgoingCallback(outgoing_callback);

  /// The controllers contain the the algorithms, the ParallelExecutor parallelizes them
  blockchain_module_ptr->consensus_executor_ =
      std::make_unique<ParallelExecutor<ConsensusController>>(cc, 1);

  /// Attach a callback to be run in the threads
  blockchain_module_ptr->consensus_executor_->attachCallback([&](DevvMessageUniquePtr p) {
    cc.consensusCallback(std::move(p));
  });


  /// Register the outgoing callback to send over zmq
  ic.registerOutgoingCallback(outgoing_callback);

  /// The controllers contain the the algorithms, the ParallelExecutor parallelizes them
  blockchain_module_ptr->internetwork_executor_ =
      std::make_unique<ParallelExecutor<InternetworkController>>(ic, 1);

  /// Attach a callback to be run in the threads
  blockchain_module_ptr->internetwork_executor_->attachCallback([&](DevvMessageUniquePtr p) {
    ic.messageCallback(std::move(p));
  });

  return (blockchain_module_ptr);
}

void BlockchainModule::handleMessage(DevvMessageUniquePtr message) {
  LogDevvMessageSummary(*message, "BlockchainModule::handleMessage()", 6);
  switch(message->message_type) {
    case eMessageType::TRANSACTION_ANNOUNCEMENT:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(TX_ANNOUNCEMNT)";
      validator_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::GET_BLOCKS_SINCE:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(GET_BLOCKS_SINCE)";
      internetwork_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::BLOCKS_SINCE:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(BLOCKS_SINCE)";
      internetwork_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::REQUEST_BLOCK:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(REQUEST_BLOCK)";
      internetwork_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::FINAL_BLOCK:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(FINAL_BLOCK)";
      consensus_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::PROPOSAL_BLOCK:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(PROPOSAL_BLOCK)";
      consensus_executor_->pushMessage(std::move(message));
      break;
    case eMessageType::VALID:
      LOG_DEBUG << "BlockchainModule::handleMessage(): push(VALID)";
      consensus_executor_->pushMessage(std::move(message));
      break;
    default:
      throw DevvMessageError("Unknown message type:"+std::to_string(message->message_type));
  }
  LOG_DEBUG << "BlockchainModule::handleMessage(): message handled!!";
}

void BlockchainModule::init()
{
  LOG_INFO << "Start BlockchainModule::init()";

  client_.attachCallback([&](DevvMessageUniquePtr p) {
    this->handleMessage(std::move(p));
  });
  client_.listenTo(app_context_.get_shard_uri());
  client_.listenTo(app_context_.get_uri());

  loopback_client_.attachCallback([&](DevvMessageUniquePtr p) {
    this->handleMessage(std::move(p));
  });
  loopback_client_.listenTo(app_context_.get_uri());

  server_.startServer();
  client_.startClient();
  loopback_client_.startClient();

  /// Initialize OpenSSL
  InitCrypto();
}

void BlockchainModule::loadHistoricChain(const std::string& working_dir) {
  LOG_DEBUG << "Looking for prior blockchain at: " << working_dir;
  Hash prev_hash = DevvHash({'G', 'e', 'n', 'e', 's', 'i', 's'});
  fs::path p(working_dir);
  if (!p.empty() && is_directory(p)) {
    std::vector<std::string> segments;
    for (auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
      segments.push_back(entry.path().string());
    }
    std::sort(segments.begin(), segments.end());
    for (auto const& seg_path : segments) {
      fs::path seg(seg_path);
      if (!seg.empty() && is_directory(seg)) {
        std::vector<std::string> files;
        for (auto& seg_entry : boost::make_iterator_range(fs::directory_iterator(seg), {})) {
          files.push_back(seg_entry.path().string());
        }
        std::sort(files.begin(), files.end());
        for (auto const& file_name : files) {
          LOG_DEBUG << "Reading " << file_name;
          std::ifstream file(file_name, std::ios::binary);
          file.unsetf(std::ios::skipws);
          std::size_t file_size;
          file.seekg(0, std::ios::end);
          file_size = file.tellg();
          file.seekg(0, std::ios::beg);
          std::vector<byte> raw;
          raw.reserve(file_size);
          raw.insert(raw.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
          if (IsBlockData(raw)) {
            InputBuffer buffer(raw);
            while (buffer.getOffset() < static_cast<size_t>(file_size)) {
              try {
                ChainState prior = final_chain_.getHighestChainState();
                auto new_block = std::make_shared<FinalBlock>(buffer, prior, keys_, mode_);
                Hash p_hash = new_block->getPreviousHash();
                if (!std::equal(std::begin(prev_hash), std::end(prev_hash), std::begin(p_hash))) {
                  LOG_FATAL
                    << "CHAINBREAK: The previous hash referenced in this block does not match the expected hash.";
                  break;
                } else {
                  prev_hash = DevvHash(new_block->getCanonical());
                  final_chain_.push_back(new_block);
                }
              } catch (const std::exception& e) {
                LOG_ERROR << "Error scanning " << file_name
                          << " skipping to next file.  Error details: " + FormatException(&e, "validator.init");
                break;
              }
            }
          } else {
            LOG_WARNING << "Working directory contained non-block binary data at: " << file_name;
          }
        }  //end file for loop
      } else {
        LOG_INFO << "Empty segment " << seg_path;
      }
    } //end segment for loop
  } else {
    LOG_INFO << "No historic blocks found, starting from Genesis.";
  }
}

bool BlockchainModule::performSanityChecks()
{
  if (!isCryptoInit) { InitCrypto(); }
  EVP_MD_CTX *ctx;
  if (!(ctx = EVP_MD_CTX_create())) {
    throw std::runtime_error("Could not create signature context!");
  }

  std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
  Hash test_hash(DevvHash(msg));

  EC_KEY *loadkey = LoadEcKey(kADDRs[1],
                              kADDR_KEYs[1],
                              app_context_.get_key_password());

  Signature sig = SignBinary(loadkey, test_hash);

  if (!VerifyByteSig(loadkey, test_hash, sig)) {
    throw std::runtime_error("Could not VerifyByteSig!");
  }
  return true;
}

void BlockchainModule::start()
{
  LOG_INFO << "Start BlockchainModule";
  init();

  try {
    consensus_executor_->start();
    internetwork_executor_->start();
    validator_executor_->start();
    LOG_INFO << "Controllers started.";
  } catch (const std::exception& e) {
    LOG_FATAL << FormatException(&e, "BlockchainModule.RunScanner");
    throw;
  }

  while (!shutdown_) {
    if (remote_blocks_ < final_chain_.size()) {
      std::vector<byte> request;
      auto request_msg = std::make_unique<DevvMessage>(app_context_.get_uri(),
          REQUEST_BLOCK, request, remote_blocks_);
      server_.queueMessage(std::move(request_msg));
      remote_blocks_ = final_chain_.size();
    }
    LOG_DEBUG << "main loop: sleeping ";
    sleep(5);
  }
}

void BlockchainModule::shutdown()
{
  request_shutdown = true;
  /// Wait for the threads to see the shutdown request
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  client_.stopClient();
  server_.stopServer();
  loopback_client_.stopClient();

  LOG_INFO << "Shutting down Devv";

}

}
