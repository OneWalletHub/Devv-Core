/*
 * DevvMPMCQueue.h implements a ring queue for pointers to Devv messages.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef CONCURRENCY_DEVVMPMCQUEUE_H_
#define CONCURRENCY_DEVVMPMCQUEUE_H_

#include <array>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <thread>

#include "blockingconcurrentqueue.h"

#include "types/DevvMessage.h"
#include "common/devv_constants.h"
#include "common/logger.h"

namespace Devv {

/**
 * Multiple producer / multiple consumer queue.
 */
class DevvMPMCQueue {
 public:
  /* Constructors/Destructors */
  DevvMPMCQueue(size_t capacity = kDEFAULT_WORKERS)
    : queue_(capacity) {
  }
  virtual ~DevvMPMCQueue() {};

  /* Disallow copy and assign */
  DevvMPMCQueue(DevvMPMCQueue const&) = delete;
  DevvMPMCQueue& operator=(DevvMPMCQueue const&) = delete;

  /** Push the pointer for a message to this queue.
   * @note once the pointer is moved the old variable becomes a nullptr.
   * @params pointer the unique_ptr for the message to send
   * @return true iff the unique_ptr was queued
   * @return false otherwise
   */
  bool push(DevvMessageUniquePtr pointer) {
    DevvMessage* ptr = pointer.release();
    //LOG_DEBUG << "pushing: " << ptr << " " << ptr->uri;
    while (!queue_.enqueue(ptr)) {
      //std::this_thread::sleep_for (std::chrono::milliseconds(1));
    }
    return true;
  }

  /** Pop a message pointer from this queue.
   * @return unique_ptr to a DevvMessage once one is queued.
   * @return nullptr, if any error
   */
  DevvMessageUniquePtr pop() {
    DevvMessage* ptr = nullptr;

    // Keep looping while wait is false (timeout) and keep_popping is true
    while (!queue_.wait_dequeue_timed(ptr, std::chrono::milliseconds(5))) {
      if (!keep_popping_) {
        LOG_INFO << "keep_popping_ == false";
        return nullptr;
      }
    }

    LOG_DEBUG << "DevvMPMCQueue::pop()ped: " << ptr << " " << ptr->uri;
    std::unique_ptr<DevvMessage> pointer(ptr);
    return pointer;
  }

  void ClearBlockers() {
    keep_popping_ = false;
  }

 private:
  moodycamel::BlockingConcurrentQueue<DevvMessage*> queue_;
  bool keep_popping_ = true;
};

} /* namespace Devv */

#endif /* CONCURRENCY_DEVVMPMCQUEUE_H_ */
