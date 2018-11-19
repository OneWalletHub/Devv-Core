/*
 * concurrency_test.cpp tests concurrency classes.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "gtest/gtest.h"

#include "concurrency/FunctionThreadPool.h"

namespace Devv {
namespace {

#define TEST_DESCRIPTION(desc) RecordProperty("concurrency unit tests", desc)

/**
 *
 * FunctionThreadPoolTest
 *
 */
class FunctionThreadPoolTest : public ::testing::Test {
 protected:
  FunctionThreadPoolTest() = default;
  ~FunctionThreadPoolTest() override = default;

  void SetUp() override {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  void TearDown() override {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

};

TEST_F(FunctionThreadPoolTest, constructor_0) {

  FunctionThreadPool ftp(1);

  EXPECT_EQ(ftp.isRunning(), false);
}

TEST_F(FunctionThreadPoolTest, add_one) {

  FunctionThreadPool ftp(1);

  int count = 0;
  auto lambda = [&]() { count++; };
  auto function = new FunctionThreadPool::Function(lambda);

  ftp.push(function);
  ftp.execute_one();

  EXPECT_EQ(count, 1);
}

TEST_F(FunctionThreadPoolTest, start_stop_0) {

  FunctionThreadPool ftp(1);

  EXPECT_EQ(ftp.isRunning(), false);
  EXPECT_EQ(ftp.start(), true);
  EXPECT_EQ(ftp.isRunning(), true);
  EXPECT_EQ(ftp.stop(), true);
  EXPECT_EQ(ftp.isRunning(), false);
}

}
}
