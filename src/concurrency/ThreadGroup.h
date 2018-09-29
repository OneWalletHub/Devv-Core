/*
 * ThreadGroup.cpp
 *
 *  Created on: 6/21/18
 *      Author: Shawn McKenney
 */
#pragma once
#include <boost/thread/thread.hpp>
#include "types/DevcashMessage.h"
#include "common/devcash_constants.h"
#include "concurrency/DevcashMPMCQueue.h"

namespace Devcash {

/**
 * The ThreadGroup enables parallel execution of a given function. When start() is called,
 * thre thread group will create 'num_threads' instances of the attached algorithm.
 */
class ThreadGroup {
 public:
  explicit ThreadGroup(size_t num_threads = kDEFAULT_WORKERS);

  /**
   * Attach a function to be called when an message arrives
   * on the queue. This function will be run in 'num_thread'
   * threads.
   * @param callback
   */
  void attachCallback(DevcashMessageCallback callback);

  /**
   * Starts the threads. Each thread will run the loop() function.
   */
  bool start();

  /**
   * Signals the threads to shutdown and join()s with them
   * @return false if the threads are shut down, true otherwise
   */
  bool stop();

  /**
   * Add a message to the input queue. The input queue is thread-safe, so multiple threads
   * can safely push messages onto the input queue.
   * @param message
   */
  void pushMessage(DevcashMessageUniquePtr message);

 private:
  /**
   * The loop function executes in each thread. It blocks on
   * incoming messages from the queue and hands each message
   * to the message_callback.
   */
  void loop();

  /// Number of threads to run in this pool
  const size_t num_threads_ = kDEFAULT_WORKERS;
  /// Thread pool
  boost::thread_group thread_pool_;
  /// Incoming data queue. Each thread will block on pop()ping from here
  DevcashMPMCQueue thread_queue_;
  /// Function to execute in each thread. Accepts a DevcashMessageUniquePtr
  DevcashMessageCallback message_callback_ = nullptr;
  /// True when running - false signals all threads to stop gracefully
  std::atomic<bool> do_run_ = ATOMIC_VAR_INIT(false);
};

} // namespace Devcash
