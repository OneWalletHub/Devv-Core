/*
 * blockchainmodule_test.cpp
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"

#include "modules/BlockchainModule.h"
#include "io/message_service.h"

using namespace Devv;

/**
 *
 * BlockchainModuleTest
 *
 */
class BlockchainModuleTest : public ::testing::Test {
  typedef std::unique_ptr<Devv::io::TransactionServer> TSPtr;
  typedef std::unique_ptr<Devv::io::TransactionClient> TCPtr;

 protected:
  BlockchainModuleTest()
      : devv_context_(1, 0, eAppMode::T2, "", "", "")
      , keys_(devv_context_)
      , context_(1)
  {
    transaction_server_ptr_ = std::make_unique<io::TransactionServer>(context_, "ipc:///tmp/junk1");
    transaction_client_ptr_ = std::make_unique<io::TransactionClient>(context_);
    loopback_client_ptr_ = std::make_unique<io::TransactionClient>(context_);

    ChainState prior;
    eAppMode mode = eAppMode::T2;

    blockchainmodule_ptr_ = BlockchainModule::Create(
        *transaction_server_ptr_,
        *transaction_client_ptr_,
        *loopback_client_ptr_,
        keys_,
        prior,
        mode,
        devv_context_,
        100000);
  }

  ~BlockchainModuleTest() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Create a default context
  DevvContext devv_context_;
  KeyRing keys_;

  zmq::context_t context_;

  TSPtr transaction_server_ptr_;
  TCPtr transaction_client_ptr_;
  TCPtr loopback_client_ptr_;

  std::unique_ptr<BlockchainModule> blockchainmodule_ptr_;
};

TEST_F(BlockchainModuleTest, constructor_0) {

}
