/*
 * ThreadGroup.cpp supports concurrency for Devv.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "ThreadGroup.h"
#include "common/logger.h"

namespace Devv {

ThreadGroup::ThreadGroup(size_t num_threads)
  : num_threads_(num_threads) {
  /// @todo mckenney - magic number
  if (num_threads > 32) {
    LOG_WARNING << "ThreadGroup created with " << num_threads_ << " threads!";
  }
  LOG_DEBUG << "ThreadGroup created with " << num_threads_ << " threads";
}

void ThreadGroup::attachCallback(DevvMessageCallback callback) {
  if (callback == nullptr) {
    throw std::runtime_error("Cannot attach a nullptr callback");
  }
  message_callback_ = callback;
}

void ThreadGroup::pushMessage(DevvMessageUniquePtr message) {
  if (message == nullptr) {
    throw std::runtime_error("pushMessage(): cannot push a nullptr to the input queue");
  }
  thread_queue_.push(std::move(message));
}

bool ThreadGroup::start() {
  if (!do_run_) {
    do_run_ = true;
    LOG_INFO << "ThreadGroup::start()";
    for (size_t w = 0; w < num_threads_; w++) {
      thread_pool_.create_thread(
          boost::bind(&ThreadGroup::loop, this));
    }
    return true;
  } else {
    LOG_ERROR << "ThreadGroup::start() - threads already running";
    return false;
  }
}

bool ThreadGroup::stop() {
  LOG_DEBUG << "ThreadGroup::stop()";
  if (!do_run_) {
    return false;
  } else {
    do_run_ = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(kMAIN_WAIT_INTERVAL));
    thread_queue_.ClearBlockers();
    thread_pool_.join_all();
    std::this_thread::sleep_for(std::chrono::milliseconds(kMAIN_WAIT_INTERVAL));
    return true;
  }
}

void ThreadGroup::loop() {
  try {
    LOG_INFO << "ThreadGroup::loop()";
    if (message_callback_ == nullptr) {
      throw std::runtime_error("ThreadGroup loop cannot execute until "
                             "a callback function is set. Call attachCallback() first please");
    }
    while (do_run_) {
      message_callback_(std::move(thread_queue_.pop()));
      LOG_INFO << "ThreadGroup::loop() - popped a message";
    }
    LOG_INFO << "ThreadGroup::loop() exit";
  } catch (const std::exception& e) {
    LOG_FATAL << FormatException(&e, "ThreadGroup::loop");
  }
}

} // namespace Devv
