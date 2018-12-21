/*
 * consensus_test.cpp tests consensus logic of Devv validators.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"

#include "primitives/blockchain.h"

namespace Devv {
namespace {

#define TEST_DESCRIPTION(desc) RecordProperty("blockchain class unit tests", desc)

/**
 *
 * BlockchainTest
 *
 */
class BlockchainTest : public ::testing::Test {
 protected:
  BlockchainTest() = default;

  ~BlockchainTest() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }
};

TEST_F(BlockchainTest, constructor_0) {
  Blockchain blockchain("my-blockchain");

  EXPECT_EQ(blockchain.getName(), "my-blockchain");
  EXPECT_EQ(blockchain.getCurrentSegmentIndex(), 0);
  EXPECT_EQ(blockchain.getCurrentSegmentHeight(), 0);
  EXPECT_EQ(blockchain.getNumTransactions(), 0);
  EXPECT_EQ(blockchain.getAvgBlocktime(), 0);
  EXPECT_EQ(blockchain.size(), 0);
  EXPECT_EQ(blockchain.getBlockVector().size(), 0);
}

// getHighestMerkleRoot
// getHighestChainState

} // namespace
} // namespace Devv
