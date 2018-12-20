/*
 * DevvRingQueue.h implements a one-way ring queue for pointers to
 * DevvMessage objects.
 *
 * This RingQueue is designed for distinct Producers and Consumers.
 * Producers and Consumers should share a reference to the queue,
 * but no object should ever call both push and pop for a given queue.
 * This RingQueue is only intended to work in one direction.
 * For bi-directional communication, construct a second queue.
 *
 * Note that this object cannot prevent the Producer from retaining access
 * to the message after it is sent. Producers should never modify or access
 * the message after pushing it.
 *
 * The point lifecycle is as follows:
 * 1.  Producer creates DevvMessage and unique_ptr to it.
 * 2.  Producer moves unique_ptr into queue using push().
 * 3.  Producer's pointer is now null and it should release the message.
 * 4.  Consumer uses pop() to get pointer from queue.
 * 5.  Queue pointer is now null
 * 6.  Consumer deletes DevvMessage when it is finished with it.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef CONCURRENCY_DEVVRINGQUEUE_H_
#define CONCURRENCY_DEVVRINGQUEUE_H_

#include <array>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "common/logger.h"
#include "common/util.h"
#include "types/DevvMessage.h"

namespace Devv {

class DevvRingQueue {
 public:
  static const int kDEFAULT_RING_SIZE = 10;

  /* Constructors/Destructors */
  DevvRingQueue() :
    kRingSize_(kDEFAULT_RING_SIZE), ptrRing_(kRingSize_) {
  }
  virtual ~DevvRingQueue() {}
  DevvRingQueue(size_t size) : kRingSize_(size), ptrRing_(kRingSize_) {}

  /* Disallow copy and assign */
  DevvRingQueue(DevvRingQueue const&) = delete;
  DevvRingQueue& operator=(DevvRingQueue const&) = delete;

  /** Push the pointer for a message to this queue.
   * @note once the pointer is moved the old variable becomes a nullptr.
   * @params pointer the unique_ptr for the message to send
   * @return true iff the unique_ptr was queued
   * @return false otherwise
   */
  bool push(std::unique_ptr<DevvMessage> pointer) {
    assert(pointer);
    CASH_TRY {
      std::unique_lock<std::mutex> lock(pushLock_);
      while (isFull_) {
        LOG_FATAL << "Queue blocked on push, run is suboptimal!";
        full_.wait(lock, [&]() {
          return(pending_ < kRingSize_);}
        );
      }
      std::unique_lock<std::mutex> eqLock(update_);
      ptrRing_.at(pushAt_) = std::move(pointer);
      pending_++;
      if (pushAt_+1 == popAt_) { isFull_ = true; }
      if (pushAt_+1 >= kRingSize_&& popAt_ == 0) { isEmpty_ = true; }
      isEmpty_ = false;
      empty_.notify_one();
      pushAt_++;
      if (pushAt_ >= kRingSize_) { pushAt_ = 0; }
      if (pushAt_ == popAt_) {
        isFull_ = true;
      } else {
        isFull_ = false;
        full_.notify_one();
      }
      return true;
    } CASH_CATCH (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "DevvRingQueue.push");
      return false;
    }
  }

  /** A better place to block when waiting to pop this queue.
   * @return unique_ptr to a DevvMessage once one is queued.
   * @return nullptr, if any error
   */
  void popGuard() {
    CASH_TRY {
      std::lock_guard<std::mutex> pop_guard(popGuardLock_);
      std::unique_lock<std::mutex> lock(popLock_);
      while (isEmpty_) {
        empty_.wait(lock, [&]() {
          return(pending_ > 0);}
        );
      }
      pending_--;
      if ((popAt_ + 1) == pushAt_) { isEmpty_ = true; }
      if ((popAt_ + 1) >= kRingSize_&& pushAt_ == 0) { isEmpty_ = true; }
    } CASH_CATCH (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "DevvRingQueue.popGuard");
    }
  }

  /** Pop a message pointer from this queue.
   * @note caller must call popGuard() before pop()
   * @return unique_ptr to a DevvMessage once one is queued.
   * @return nullptr, if any error
   */
  std::unique_ptr<DevvMessage> pop() {
    CASH_TRY {
      std::unique_lock<std::mutex> eqLock(update_);
      std::unique_ptr<DevvMessage> out = std::move(ptrRing_.at(popAt_));
      isFull_ = false;
      full_.notify_one();
      popAt_++;
      if (popAt_ >= kRingSize_) { popAt_ = 0; }
      if (popAt_ == pushAt_) {
        isEmpty_ = true;
      } else {
        isEmpty_ = false;
        empty_.notify_one();
      }
      assert(out);
      return out;
    } CASH_CATCH (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "DevvRingQueue.pop");
      return nullptr;
    }
  }

  /** Notify all threads blocking on this queue that they should stop.
   * @return unique_ptr to a DevvMessage once one is queued.
   * @return nullptr, if any error
   */
  void clearBlockers() {
    stopNow_ = true;
    isEmpty_ = false;
    isFull_ = false;
    pending_ = 0;
    empty_.notify_all();
    full_.notify_all();
  }

 private:
  int kRingSize_;
  std::vector<std::unique_ptr<DevvMessage>> ptrRing_;
  int pushAt_ = 0;
  int popAt_ = 0;
  std::mutex pushLock_; //orders access to push()
  std::mutex popLock_; //orders access to pop()
  std::mutex update_; //stops threads from updating members simultaneously
  std::mutex popGuardLock_;
  int pending_ = 0;
  bool isEmpty_ = true;
  bool isFull_ = false;
  std::condition_variable empty_;
  std::condition_variable full_;
  bool stopNow_ = false;
};

} /* namespace Devv */

#endif /* CONCURRENCY_DEVVRINGQUEUE_H_ */
