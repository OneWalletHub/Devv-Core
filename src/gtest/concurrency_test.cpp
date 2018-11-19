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

TEST_F(FunctionThreadPoolTest, push_0) {

  FunctionThreadPool ftp(3);
  EXPECT_EQ(ftp.start(), true);

  std::atomic_int count = ATOMIC_VAR_INIT(0);
  auto lambda = [&]() { count++; };

  for (int i = 0; i < 10; i++) {
    auto function = new FunctionThreadPool::Function(lambda);
    ftp.push(function);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(ftp.num_pushed(), 10);
  EXPECT_EQ(ftp.num_popped(), 10);
  EXPECT_EQ(ftp.stop(), true);
}

TEST_F(FunctionThreadPoolTest, push_1) {
  class ResourceCounter {
   public:
    explicit ResourceCounter(std::atomic_int& inc, std::atomic_int& dec)
    : dec_(dec) {
      inc++;
    }

    ~ResourceCounter() { dec_++; }

    std::atomic_int& dec_;
  };

  std::atomic_int num_incs = ATOMIC_VAR_INIT(0);
  std::atomic_int num_decs = ATOMIC_VAR_INIT(0);


  // Create and start pool
  FunctionThreadPool ftp(3);
  EXPECT_EQ(ftp.start(), true);

  int num_execs = 15;
  for (int i = 0; i < num_execs; i++) {
    // create a resource counter
    auto counter = std::make_shared<ResourceCounter>(num_incs, num_decs);
    // capture counter by value
    auto lambda = [counter]() { auto c = counter; };
    // create a function pointer
    auto function = new FunctionThreadPool::Function(lambda);
    // push onto the thread pool
    ftp.push(function);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(1000));
  EXPECT_EQ(ftp.num_pushed(), num_execs);
  EXPECT_EQ(ftp.num_popped(), num_execs);
  EXPECT_EQ(num_incs, num_execs);
  EXPECT_EQ(num_decs, num_execs);

  EXPECT_EQ(ftp.stop(), true);
}

}
}
