/*
 * FunctionThreadPool.h supports parallel execution of functions.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <functional>
#include <chrono>
#include <thread>

#include <boost/lockfree/queue.hpp>
#include <boost/thread/thread.hpp>

#include "common/devv_constants.h"
#include "common/logger.h"

namespace Devv {

/**
 * The FunctionThreadPool enables parallel execution of functions, lambdas and closures. When
 * start() is called, the thread group will create 'num_threads' executors. The executors will block on a
 * boost::lockfree::queue waiting for functions to execute.
 *
 * NOTE: boost::lockfree::queue requires trivial constructors, destructors and assignment operators,
 * therefore std::function()s cannot be queued. FunctionThreadPool queues pointers to std::functions
 * and will delete the function after execution.
 */
class FunctionThreadPool {

 public:
  typedef std::function<void()> Function;

  /**
   * Constructor
   * @param num_threads
   */
  explicit FunctionThreadPool(size_t num_threads = kDEFAULT_WORKERS)
      : num_threads_(num_threads)
  {
  }

  /**
   * Returns true if the thread pool is running
   * @return
   */
  bool isRunning() {
    return do_run_;
  }

  /**
   * Starts the threads. Each thread will run the loop() function.
   * @return true if the threads are created, false if the pool is already started
   */
  bool start() {
    if (!do_run_) {
      do_run_ = true;
      LOG_INFO << "FunctionThreadPool::start()";
      for (size_t w = 0; w < num_threads_; w++) {
        thread_pool_.create_thread(
          boost::bind(&FunctionThreadPool::loop, this));
      }
      return true;
    } else {
      // already running
      LOG_WARNING << "FunctionThreadPool::start() - threads already running";
      return false;
    }
  }

  /**
   * Signals the threads to shutdown and join()s with them
   * @return false if the threads are shut down, true otherwise
   */
  bool stop() {
    if (!do_run_) {
      // already stopped
      LOG_WARNING << "FunctionThreadPool::stop() - threads already stopped";
      return false;
    } else {
      // stop me
      LOG_WARNING << "FunctionThreadPool::stop() - stopping";
      do_run_ = false;
      std::this_thread::sleep_for(std::chrono::milliseconds(kMAIN_WAIT_INTERVAL));
      thread_pool_.join_all();
      return true;
    }
  }

  /**
   * Add a function to the input queue. The input queue is thread-safe, so multiple threads
   * can safely push functions onto the input queue.
   * @param function Raw pointer to a Function object (std::function<void()>)
   */
  void push(Function* function) {
    while (!queue_.push(function)) {
      LOG_WARNING << "FunctionThreadPool::push(): push failed!";
      std::this_thread::sleep_for(std::chrono::milliseconds(kMAIN_WAIT_INTERVAL));
    }
    num_pushed_++;
  }

  /**
   * Pops one function off of the queue and executes it
   */
  void execute_one() {
    Function* function = nullptr;
    while (!queue_.pop(function)) {
      // break for shutdown
      if (!do_run_) break;
      // queue is empty - sleep a bit and try again
      std::this_thread::sleep_for(std::chrono::milliseconds(kMAIN_WAIT_INTERVAL));
    }
    if (function) {
      function->operator()();
      num_popped_++;
      delete function;
    } else {
      LOG_DEBUG << "FunctionThreadPool::execute_one(): function == nullptr";
    }
  }

  /**
   * Returns the number of functions pushed onto the queue
   * @return number of function pointers pushed onto the queue
   */
  uint64_t num_pushed() {
    return num_pushed_;
  }

  /**
   * Returns the number of functions popped and executed
   * @return number of functions popped/executed
   */
  uint64_t num_popped() {
    return num_popped_;
  }

 private:
  /**
   * The loop function executes in each thread. It blocks on
   * incoming messages from the queue and hands each message
   * to the message_callback.
   */
  void loop() {
    while (do_run_) {
      LOG_DEBUG << "FunctionThreadPool::loop(): executing next function";
      execute_one();
      LOG_DEBUG << "FunctionThreadPool::loop(): execute_one() complete";
    }
  }

 private:
  /// Thread-safe lock-free queue of pending functions
  boost::lockfree::queue<Function*, boost::lockfree::capacity<1024> > queue_;
  /// Number of threads to run in this pool
  const size_t num_threads_ = kDEFAULT_WORKERS;
  /// Thread pool
  boost::thread_group thread_pool_;
  /// True when running - false signals all threads to stop gracefully
  std::atomic<bool> do_run_ = ATOMIC_VAR_INIT(false);
  /// Number of function pointers pushed onto the queue
  std::atomic<uint64_t> num_pushed_ = ATOMIC_VAR_INIT(0);
  /// Number of function pointers popped off the queue
  std::atomic<uint64_t> num_popped_ = ATOMIC_VAR_INIT(0);
};

} // namespace Devv
