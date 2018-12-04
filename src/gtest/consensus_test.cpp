/*
 * consensus_test.cpp tests consensus logic of Devv validators.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"

#include "primitives/Summary.h"
#include "primitives/Tier1Transaction.h"
#include "primitives/Tier2Transaction.h"
#include "primitives/Transfer.h"
#include "primitives/json_interface.h"

#include "consensus/chainstate.h"
#include "consensus/UnrecordedTransactionPool.h"
#include "consensus/tier2_message_handlers.h"

namespace Devv {
namespace {

#define TEST_DESCRIPTION(desc) RecordProperty("consensus algorithm unit tests", desc)

/**
 *
 * ChainStateTest
 *
 */
class ChainStateTest : public ::testing::Test {
 protected:
  ChainStateTest() : t1_context_0_(0, 0, Devv::eAppMode::T1,
                                   "", "", ""), keys_(t1_context_0_) {
    for (int i = 0; i < 4; ++i) {
      keys_.addWalletKeyPair(kADDRs.at(i), kADDR_KEYs.at(i), "password");
    }
    keys_.setInnKeyPair(kINN_ADDR, kINN_KEY, "password");
    for (int i = 0; i < 3; ++i) {
      keys_.addNodeKeyPair(kNODE_ADDRs.at(i), kNODE_KEYs.at(i), "password");
    }
  }

  ~ChainStateTest() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Create a default context
  Devv::DevvContext t1_context_0_;

  Devv::KeyRing keys_;
};

TEST_F(ChainStateTest, constructor_0) {
  CoinMap coin_map;
  DelayedItem delayed_item;
  AddToCoinMap(10, delayed_item, coin_map);

  ChainState chain_state;

  EXPECT_EQ(chain_state.getStateMap().size(), 0);
}

TEST_F(ChainStateTest, addCoin_0) {

  SmartCoin coin(keys_.getWalletAddr(0), 10, 10);

  ChainState chain_state;
  chain_state.addCoin(coin);

  EXPECT_EQ(chain_state.getStateMap().size(), 1);
}

/*
 *
 * The following tests create a mock chain in a single
 * threaded test environment. It supports the testing of the full validation
 * process by chaining the modules together as they are done on the live system.
 *
 */

/**
 * Create a CREATE transaction from the INN
 *
 * @param keys KeyRing holding test keys
 * @param amount Number of coins to create
 * @return
 */
Tier2TransactionPtr CreateInnTransaction(const KeyRing& keys, int64_t amount) {
  size_t addr_count = keys.CountWallets();
  Address inn_addr = keys.getInnAddr();

  std::string nonce_str = "Here is a very big test nonce for the UnrecordedTransactionPool class.";
  std::vector<byte> nonce_bin(nonce_str.begin(), nonce_str.end());

  std::vector<Transfer> xfers;
  Transfer inn_transfer(inn_addr, 0, -1 * addr_count * amount, 0);
  xfers.push_back(inn_transfer);
  for (size_t i = 0; i < addr_count; ++i) {
    Transfer transfer(keys.getWalletAddr(i), 0, amount, 0);
    xfers.push_back(transfer);
  }

  Tier2TransactionPtr inn_tx = std::make_unique<Tier2Transaction>(eOpType::Create, xfers, nonce_bin,
                                                                  keys.getKey(inn_addr), keys);

  return inn_tx;
}

/**
 * Create a vector of txes with a single
 * @param keys
 * @return
 */
Tier2TransactionPtr CreateTestTransaction(const KeyRing& keys, int send_amount = -1, int recv_amount = 1) {
  std::vector<TransactionPtr> txs;

  Transfer receiver(keys.getWalletAddr(0), 0, recv_amount, 0);
  Transfer sender(keys.getWalletAddr(1), 0, send_amount, 0);

  std::vector<Transfer> xfers;
  xfers.push_back(sender);
  xfers.push_back(receiver);

  uint64_t nonce = 101101101;
  std::vector<byte> nonce_bin;
  Uint64ToBin(nonce, nonce_bin);
  auto t2x = std::make_unique<Tier2Transaction>(
      eOpType::Exchange, xfers,
      nonce_bin,
      keys.getWalletKey(1), keys);

  return t2x;
}


/**
 * TestHandler
 *
 * Base class for ConsensusTest handler classes
 */
class TestHandler {
 public:
  TestHandler() = delete;

  TestHandler(unsigned int node,
              unsigned int shard,
              eAppMode mode = eAppMode::T2)
      : context_(node, shard, mode, "", "", "")
      , keys_(std::make_unique<KeyRing>(context_))
      //, proposal_chain_("prop-chain")
      , final_chain_("final-chain")
      , chain_state_()
      , utx_pool_ptr_() {
    for (int i = 0; i < 4; ++i) {
      keys_->addWalletKeyPair(kADDRs.at(i), kADDR_KEYs.at(i), "password");
    }
    keys_->setInnKeyPair(kINN_ADDR, kINN_KEY, "password");
    for (int i = 0; i < 3; ++i) {
      keys_->addNodeKeyPair(kNODE_ADDRs.at(i), kNODE_KEYs.at(i), "password");
    }
    utx_pool_ptr_ = std::make_unique<UnrecordedTransactionPool>(chain_state_, eAppMode::T2, 100);
  }

  bool setCompletionCallback0(DevvMessageCallback callback) {
    completion_cb0_ = callback;
    return true;
  }

  bool setCompletionCallback1(DevvMessageCallback callback) {
    completion_cb1_ = callback;
    return true;
  }

  const KeyRing& getKeyRing() const {
    return *keys_;
  }

  void generateKeyRing(const DevvContext& context) {
    keys_ = std::make_unique<KeyRing>(context);
  }

  virtual ~TestHandler() = default;

  // Create a default context
  DevvContext context_;
  std::unique_ptr<KeyRing> keys_;
  //Blockchain proposal_chain_;
  Blockchain final_chain_;

  ChainState chain_state_;
  std::unique_ptr<UnrecordedTransactionPool> utx_pool_ptr_;

  DevvMessageCallback completion_cb0_;
  DevvMessageCallback completion_cb1_;
};


/**
 * TestTransactionHandler
 */
class TestTransactionHandler : public TestHandler {
 public:
  TestTransactionHandler() = delete;

  TestTransactionHandler(unsigned int node,
                         unsigned int shard)
  : TestHandler(node, shard) {

  }
  ~TestTransactionHandler() final = default;

 public:
  size_t addTransaction(TransactionPtr transaction) {
    tx_vector_.push_back(std::move(transaction));
    return tx_vector_.size();
  }

  void announceTransactions() {
    std::unique_ptr<Tier2Transaction> inn_tx = CreateInnTransaction(*keys_, 100);
    EXPECT_EQ(inn_tx->getOperation(), eOpType::Create);

    std::vector<TransactionPtr> inn_tx_vector;

    inn_tx_vector.push_back(std::move(inn_tx));

    utx_pool_ptr_->addTransactions(inn_tx_vector, *keys_);

    EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

    auto proposal = CreateNextProposal(*keys_,
                                       final_chain_,
                                       *utx_pool_ptr_,
                                       context_);

    EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

    InputBuffer buffer(proposal);
    ProposedBlock to_validate(ProposedBlock::Create(buffer, chain_state_, *keys_, utx_pool_ptr_->get_transaction_creation_manager()));

    EXPECT_EQ(to_validate.getVersion(), 0);

    std::cout << "VERSION: " << int(to_validate.getVersion()) << " -- " << std::endl;

    auto valid = to_validate.validate(*keys_);
    auto sign = to_validate.signBlock(*keys_, 1);
    EXPECT_TRUE(valid);
    EXPECT_TRUE(sign);

    size_t block_height = final_chain_.size();

    EXPECT_EQ(final_chain_.size(), 0);
    //EXPECT_EQ(proposal_chain_.size(), 0);

/*
 * auto propose_msg = std::make_unique<DevvMessage>(context_.get_shard_uri()
        , PROPOSAL_BLOCK
        , proposal
        , ((block_height+1)
            + (context_.get_current_node()+1)*1000000));
    // FIXME (spm): define index value somewhere
    LogDevvMessageSummary(*propose_msg, "CreateNextProposal");
    validation_handler_cb_(std::move(propose_msg));
*/
    auto publish_msg = std::make_unique<DevvMessage>(context_.get_shard_uri()
        , PROPOSAL_BLOCK
        , proposal
        , ((block_height+1)
            + (context_.get_current_node()+1)*1000000));
    completion_cb0_(std::move(publish_msg));
  }

  void handleValidationBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestTransactionHandler::handleValidationBlock");
    HandleValidationBlock(std::move(message), context_, final_chain_, *utx_pool_ptr_, completion_cb1_);
  }

  /*
   * void handleProposerCallback(DevvMessageUniquePtr) {
    completion_cb_(std::move(validation_msg));
  }
  */

 private:
  std::vector<TransactionPtr> tx_vector_;
  DevvMessageCallback validation_handler_cb_;
};
typedef std::unique_ptr<TestTransactionHandler> TestTransactionHandlerPtr;


/**
 * TestProposalHandler
 */
class TestProposalHandler : public TestHandler {
 public:
  TestProposalHandler() = delete;
  TestProposalHandler(unsigned int node,
                      unsigned int shard)
      : TestHandler(node, shard) {

  }

  ~TestProposalHandler() final = default;

  void handleProposalBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestProposalHandler::handleProposalBlock");
    HandleProposalBlock(std::move(message),
                        context_,
                        *keys_,
                        final_chain_,
                        *utx_pool_ptr_,
                        completion_cb0_);
  }

  void handleFinalBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestProposalHandler::handleFinalizationBlock");
    HandleFinalBlock(std::move(message),
                     context_,
                     *keys_,
                     final_chain_,
                     *utx_pool_ptr_,
                     completion_cb1_);
  }
};
typedef std::unique_ptr<TestProposalHandler> TestProposalHandlerPtr;


/**
 * TestValidationHandler
 */
/*class TestValidationHandler : public TestHandler {
 public:
  TestValidationHandler() = delete;
  TestValidationHandler(unsigned int node,
                        unsigned int shard)
      : TestHandler(node, shard) {

  }
  ~TestValidationHandler() final = default;

 public:

  void haltValidMessage(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestValidationHandler::haltValidMessage");
  }

  void handleProposalBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestValidationHandler::handleProposalBlock");
    DevvMessageCallback halt_cb = [this](DevvMessageUniquePtr p) { this->haltValidMessage(std::move(p)); };

    HandleProposalBlock(std::move(message),
                        context_,
                        *keys_,
                        final_chain_,
                        utx_pool_ptr_, halt_cb);
  }

  void handleValidationBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestValidationHandler::handleValidationBlock");

    HandleValidationBlock(std::move(message), context_, final_chain_, *utx_pool_ptr_, completion_cb_);
  }
};
typedef std::unique_ptr<TestValidationHandler> TestValidationHandlerPtr;
*/

/**
 * TestFinalizationHandler
 */
/*class TestFinalizationHandler : public TestHandler {
 public:
  TestFinalizationHandler() = delete;
  TestFinalizationHandler(unsigned int node,
                          unsigned int shard)
      : TestHandler(node, shard) {

  }
  ~TestFinalizationHandler() final = default;

  void handleFinalBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "TestFinalizationHandler::handleFinalBlock");
    HandleFinalBlock(std::move(message),
                     context_,
                     *keys_,
                     final_chain_,
                     *utx_pool_ptr_,
                     completion_cb_);
    LogDevvMessageSummary(*message, "TestFinalizationHandler::handleFinalBlock");
  }

 private:

};
typedef std::unique_ptr<TestFinalizationHandler> TestFinalizationHandlerPtr;
*/

/**
 * ConsensusTestManager
 *
 * The ConsensusTestManager instatiates and initializes the blockchain module handlers
 * This tests exercises the system in a very similar fashion as it runs live
 */
class ConsensusTestManager {
 public:
  ConsensusTestManager(unsigned int shard_num,
                       unsigned int proposer_node_num = 0,
                       unsigned int validator_node_num = 1)
      : proposer_node_num_(proposer_node_num)
      , validator_node_num_(validator_node_num)
      , shard_num_(shard_num) {

  }
  ~ConsensusTestManager() = default;

  /**
   * Initializes the Devv module handlers
   */
  void initializeHandlers() {
    transaction_handler_ = std::make_unique<TestTransactionHandler>(proposer_node_num_, shard_num_);
    proposal_handler_ = std::make_unique<TestProposalHandler>(validator_node_num_, shard_num_);
    initializeFinalizationHandler();
    initializeValidationHandler();
    initializeProposalHandler();
    initializeTransactionHandler();
  }

  void sendTestTransactions(uint64_t coin_num, int64_t amount) {
    initializeHandlers();
    transaction_handler_->announceTransactions();
    std::cout << "Done" << std::endl;
  }

  /**
   * Initializes the TransactionHandler
   */
  void initializeTransactionHandler() {
    DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->proposal_handler_->handleProposalBlock(std::move(p)); };
    transaction_handler_->setCompletionCallback0(cb);
  }

  /**
   * Initializes the ProposalHandler
   */
  void initializeProposalHandler() {
    DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->transaction_handler_->handleValidationBlock(std::move(p)); };
    proposal_handler_->setCompletionCallback0(cb);
  }

  /**
   * Initializes the ValidationHandler
   */
  void initializeValidationHandler() {
    DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->proposal_handler_->handleFinalBlock(std::move(p)); };
    transaction_handler_->setCompletionCallback1(cb);
  }

  /**
   * Initializes the FinalizationHandler
   */
  void initializeFinalizationHandler() {
    DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->finalDevvMessageHandler(std::move(p)); };
    proposal_handler_->setCompletionCallback1(cb);
  }

  /**
   * Initializes the test method to inxest the final DevvMessage
   *
   * @param message The finalized DevvMessage from the FinalizationHandler
   */
  void finalDevvMessageHandler(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "finalDevvMessageHandler");
  }

 private:
  ConsensusTestManager() = delete;

  unsigned int proposer_node_num_;
  unsigned int validator_node_num_;

  unsigned int shard_num_;

  TestTransactionHandlerPtr transaction_handler_;
  TestProposalHandlerPtr proposal_handler_;
  //TestValidationHandlerPtr validation_handler_;
  //TestFinalizationHandlerPtr finalization_handler_;
};

/**
 * ConsensusTestRunner
 */
class ConsensusTestRunner : public ::testing::Test {
 protected:
  ~ConsensusTestRunner() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }
};

TEST_F(ConsensusTestRunner, announce_0) {
  ConsensusTestManager test_manager(1);
  test_manager.sendTestTransactions(0, 2);
}

/*
 * UnrecordedTransactionPool Test Cases
 *
 * The following tests exercise the UnrecordedTransactionPool
 *
 */

/**
 * UnrecordedTransactionPoolTest
 */
class UnrecordedTransactionPoolTest : public ::testing::Test {
 protected:
  UnrecordedTransactionPoolTest()
      : t2_context_(1, 0, eAppMode::T2, "", "", "")
      , keys_(t2_context_)
      , proposal_chain_("prop-chain")
      , final_chain_("final-chain")
      , chain_state_()
      , utx_pool_ptr_() {
    for (int i = 0; i < 4; ++i) {
      keys_.addWalletKeyPair(kADDRs.at(i), kADDR_KEYs.at(i), "password");
    }
    keys_.setInnKeyPair(kINN_ADDR, kINN_KEY, "password");
    for (int i = 0; i < 3; ++i) {
      keys_.addNodeKeyPair(kNODE_ADDRs.at(i), kNODE_KEYs.at(i), "password");
    }
    utx_pool_ptr_ = std::make_unique<UnrecordedTransactionPool>(chain_state_, eAppMode::T2, 100);
  }

  ~UnrecordedTransactionPoolTest() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  std::vector<byte> createTestProposal() {
    return CreateNextProposal(keys_,
                              proposal_chain_,
                              *utx_pool_ptr_,
                              t2_context_);
  }

  void handleValidationBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "UnrecordedTransactionPoolTest::handleValidationBlock");

    DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->handleFinalBlock(std::move(p)); };

    HandleValidationBlock(std::move(message), t2_context_, final_chain_, *utx_pool_ptr_, cb);
  }

  void handleFinalBlock(DevvMessageUniquePtr message) {
    LogDevvMessageSummary(*message, "UnrecordedTransactionPoolTest::handleFinalBlock");
  }

  // Create a default context
  DevvContext t2_context_;
  KeyRing keys_;
  Blockchain proposal_chain_;
  Blockchain final_chain_;

  ChainState chain_state_;
  std::unique_ptr<UnrecordedTransactionPool> utx_pool_ptr_;
};

TEST_F(UnrecordedTransactionPoolTest, constructor_0) {
  CoinMap coin_map;
  DelayedItem delayed_item;
  AddToCoinMap(10, delayed_item, coin_map);

  ChainState chain_state;

  EXPECT_EQ(chain_state.getStateMap().size(), 0);

  //utx_pool_ptr_ = std::make_unique<UnrecordedTransactionPool>(chain_state, eAppMode::T2, 100);

  EXPECT_EQ(utx_pool_ptr_->getMode(), eAppMode::T2);
}

TEST_F(UnrecordedTransactionPoolTest, addTransaction_0) {
  std::unique_ptr<Tier2Transaction> inn_tx = CreateInnTransaction(keys_, 100);
  EXPECT_EQ(inn_tx->getOperation(), eOpType::Create);

  std::vector<TransactionPtr> inn_tx_vector;
  inn_tx_vector.push_back(std::move(inn_tx));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();
}

TEST_F(UnrecordedTransactionPoolTest, isNullProposal_0) {
  auto t2x = CreateInnTransaction(keys_, 100);

  std::vector<TransactionPtr> inn_tx_vector;
  inn_tx_vector.push_back(std::move(t2x));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);
}

TEST_F(UnrecordedTransactionPoolTest, validate_0) {
  auto t2x = CreateInnTransaction(keys_, 100);

  std::vector<TransactionPtr> tx_vector;
  tx_vector.push_back(std::move(t2x));

  utx_pool_ptr_->addTransactions(tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

  InputBuffer buffer(proposal);
  ProposedBlock to_validate(ProposedBlock::Create(buffer,
                                                  chain_state_,
                                                  keys_,
                                                  utx_pool_ptr_->get_transaction_creation_manager()));
  EXPECT_EQ(to_validate.getVersion(), 0);

  auto valid = to_validate.validate(keys_);

  EXPECT_TRUE(valid);
}


TEST_F(UnrecordedTransactionPoolTest, validate_1) {
  auto t2x = CreateInnTransaction(keys_, 100);

  std::vector<TransactionPtr> inn_tx_vector;
  inn_tx_vector.push_back(std::move(t2x));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

  InputBuffer buffer(proposal);
  ProposedBlock to_validate(ProposedBlock::Create(buffer, chain_state_, keys_, utx_pool_ptr_->get_transaction_creation_manager()));
  auto valid = to_validate.validate(keys_);
  size_t node_num = 2;
  auto sign = to_validate.signBlock(keys_, node_num);
  EXPECT_TRUE(valid);
  EXPECT_TRUE(sign);
}

TEST_F(UnrecordedTransactionPoolTest, assymetric_0) {
  EXPECT_THROW(CreateTestTransaction(keys_, 2, 2), std::runtime_error);
}

TEST_F(UnrecordedTransactionPoolTest, finalize_0) {
  auto t2x = CreateInnTransaction(keys_, 100);

  std::vector<TransactionPtr> inn_tx_vector;
  inn_tx_vector.push_back(std::move(t2x));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

  InputBuffer buffer(proposal);
  ProposedBlock to_validate(ProposedBlock::Create(buffer, chain_state_, keys_, utx_pool_ptr_->get_transaction_creation_manager()));
  auto valid = to_validate.validate(keys_);
  size_t node_num = 2;
  auto sign = to_validate.signBlock(keys_, node_num);
  LOG_INFO << "Proposal: "+GetJSON(to_validate);
  EXPECT_TRUE(valid);
  EXPECT_TRUE(sign);

  InputBuffer validation(to_validate.getValidationData());

  EXPECT_TRUE(utx_pool_ptr_->checkValidation(validation, t2_context_));
/*
    //block can be finalized, so finalize
    LOG_DEBUG << "Ready to finalize block.";
    FinalPtr top_block = std::make_shared<FinalBlock>(utx_pool_ptr_->FinalizeLocalBlock());
    proposal_chain_.push_back(top_block);
*/
}

TEST_F(UnrecordedTransactionPoolTest, finalize_inn_tx) {
  std::unique_ptr<Tier2Transaction> inn_tx = CreateInnTransaction(keys_, 100);
  EXPECT_EQ(inn_tx->getOperation(), eOpType::Create);

  std::vector<TransactionPtr> inn_tx_vector;

  inn_tx_vector.push_back(std::move(inn_tx));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

  InputBuffer buffer(proposal);
  ProposedBlock to_validate(ProposedBlock::Create(buffer, chain_state_, keys_, utx_pool_ptr_->get_transaction_creation_manager()));

  EXPECT_EQ(to_validate.getVersion(), 0);

  std::cout << "VERSION: " << int(to_validate.getVersion()) << " -- " << std::endl;

  auto valid = to_validate.validate(keys_);
  size_t node_num = 2;
  auto sign = to_validate.signBlock(keys_, node_num);
  EXPECT_TRUE(valid);
  EXPECT_TRUE(sign);

  size_t block_height = final_chain_.size();

  EXPECT_EQ(final_chain_.size(), 0);
  EXPECT_EQ(proposal_chain_.size(), 0);

  // Create message
  auto propose_msg =
      std::make_unique<DevvMessage>(t2_context_.get_shard_uri(),
                                    PROPOSAL_BLOCK,
                                    proposal,
                                    ((block_height + 1) + (t2_context_.get_current_node() + 1) * 1000000));

  DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->handleValidationBlock(std::move(p)); };

  DevvContext sign_context(2, 1, Devv::eAppMode::T2, "", "", "");
  HandleProposalBlock(std::move(propose_msg),
                      sign_context,
                      keys_,
                      proposal_chain_,
                      *utx_pool_ptr_, cb);
}

TEST_F(UnrecordedTransactionPoolTest, finalize_tx_1) {
auto t2x = CreateTestTransaction(keys_, -2, 2);
std::unique_ptr<Tier2Transaction> inn_tx = CreateInnTransaction(keys_, 100);
EXPECT_EQ(inn_tx->getOperation(), eOpType::Create);

std::vector<TransactionPtr> inn_tx_vector;
inn_tx_vector.push_back(std::move(inn_tx));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

auto proposal = createTestProposal();

EXPECT_EQ(ProposedBlock::isNullProposal(proposal), false);

InputBuffer buffer(proposal);
ProposedBlock to_validate(ProposedBlock::Create(buffer, chain_state_, keys_, utx_pool_ptr_->get_transaction_creation_manager()));
auto valid = to_validate.validate(keys_);
size_t node_num = 2;
auto sign = to_validate.signBlock(keys_, node_num);
EXPECT_TRUE(valid);
EXPECT_TRUE(sign);

size_t block_height = final_chain_.size();

EXPECT_EQ(final_chain_.size(), 0);
EXPECT_EQ(proposal_chain_.size(), 0);

// Create message
auto propose_msg =
    std::make_unique<DevvMessage>(t2_context_.get_shard_uri(),
                                  PROPOSAL_BLOCK,
                                  proposal,
                                  ((block_height + 1) + (t2_context_.get_current_node() + 1) * 1000000));

DevvMessageCallback cb = [this](DevvMessageUniquePtr p) { this->handleValidationBlock(std::move(p)); };
DevvContext sign_context(2, 1, Devv::eAppMode::T2, "", "", "");
HandleProposalBlock(std::move(propose_msg), sign_context, keys_, proposal_chain_, *utx_pool_ptr_, cb);
}

TEST_F(UnrecordedTransactionPoolTest, DISABLED_proposal_stream_0) {
  auto t2x = CreateTestTransaction(keys_, -2, 2);

  std::vector<TransactionPtr> inn_tx_vector;
  inn_tx_vector.push_back(std::move(t2x));

  utx_pool_ptr_->addTransactions(inn_tx_vector, keys_);

  EXPECT_EQ(utx_pool_ptr_->numPendingTransactions(), 1);

  auto proposal = createTestProposal();

  InputBuffer buffer(proposal);
  auto proposal2 = ProposedBlock::Create(buffer, chain_state_, keys_, utx_pool_ptr_->get_transaction_creation_manager());

  EXPECT_EQ(proposal, proposal2.getCanonical());
}

} // namespace
} // namespace Devv
