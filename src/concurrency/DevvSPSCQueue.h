/*
 * DevvSPSCQueue.h implements a ring queue for pointers to Devv messages.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef CONCURRENCY_DEVVSPSCQUEUE_H_
#define CONCURRENCY_DEVVSPSCQUEUE_H_

#include <array>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <thread>

#include <boost/lockfree/spsc_queue.hpp>

#include "types/DevvMessage.h"
#include "common/logger.h"

namespace Devv {

/**
 * Single producer / single consumer queue.
 */
class DevvSPSCQueue {
 public:
  /* Constructors/Destructors */
  DevvSPSCQueue() {
  }
  virtual ~DevvSPSCQueue() {};

  /* Disallow copy and assign */
  DevvSPSCQueue(DevvSPSCQueue const&) = delete;
  DevvSPSCQueue& operator=(DevvSPSCQueue const&) = delete;

  /** Push the pointer for a message to this queue.
   * @note once the pointer is moved the old variable becomes a nullptr.
   * @params pointer the unique_ptr for the message to send
   * @return true iff the unique_ptr was queued
   * @return false otherwise
   */
  bool push(std::unique_ptr<DevvMessage> pointer) {
    DevvMessage* ptr = pointer.release();
    while (!spsc_queue_.push(ptr)) {
      std::this_thread::sleep_for (std::chrono::milliseconds(1));
    }
    return true;
  }

  /** Pop a message pointer from this queue.
   * @return unique_ptr to a DevvMessage once one is queued.
   * @return nullptr, if any error
   */
  std::unique_ptr<DevvMessage> pop() {
    DevvMessage* ptr;

    while (!spsc_queue_.pop(ptr)) {
      std::this_thread::sleep_for (std::chrono::milliseconds(1));
    }

    std::unique_ptr<DevvMessage> pointer(ptr);
    return pointer;
  }

 private:
  boost::lockfree::spsc_queue<DevvMessage*, boost::lockfree::capacity<1024> > spsc_queue_;
};

} /* namespace Devv */

#endif /* CONCURRENCY_DEVVSPSCQUEUE_H_ */
