/*
 * blockservice_test.cpp
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"

#include "io/block_service.h"

using namespace Devv;

/**
 *
 * BlockServiceTest
 *
 */
class BlockServiceTest : public ::testing::Test {
 protected:
  BlockServiceTest()
      : devv_context_(1, 0, eAppMode::T2, "", "", "password")
      , keys_(devv_context_)
  {
  }

  ~BlockServiceTest() override = default;

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

  std::unique_ptr<DevvQueryBlockReader> blockservice_ptr_;
};

TEST_F(BlockServiceTest, constructor_0) {
  EXPECT_EQ(blockservice_ptr_, nullptr);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
