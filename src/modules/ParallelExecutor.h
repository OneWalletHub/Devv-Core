/*
 * ParallelExecutor.h executes controllers in parallel
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "common/devv_context.h"

#include "concurrency/ThreadGroup.h"
#include "types/DevvMessage.h"

namespace Devv {

/**
 * The ParallelExecutor is a template class that can execute
 * many instances of the Controller algorithm in parallel.
 * @tparam Controller
 */
template <typename Controller>
class ParallelExecutor {
 public:
  /**
   * Constructor. Takes a reference to a controller object of type Controller, the context
   * and the number of parallel instances to run.
   *
   * @param controller
   * @param context
   * @param num_threads
   */
  ParallelExecutor(Controller& controller,
                     size_t num_threads)
      : controller_(controller)
      , thread_group_(num_threads)
  {
    LOG_DEBUG << "ParallelExecutor(" << num_threads << ")";
  }

  /**
   * Attach a callback to be executed in parallel
   * @param callback
   */
  void attachCallback(DevvMessageCallback callback) {
    if (callback == nullptr) {
      throw std::runtime_error("Cannot attach a nullptr callback");
    }
    LOG_DEBUG << "attachCallback()";
    thread_group_.attachCallback(callback);
  }

  /**
   * Initializes the executor and calls init() on the controller algorithm.
   */
  void init() {
    LOG_DEBUG << "init()";
    controller_.init();
  }

  /**
   * Performs a set of sanity checks to ensure the controller is ready to run.
   * Calls sanityChecks() on the controller. An exception is thrown if the
   * sanity checks fail.
   */
  void sanityChecks() {
    LOG_DEBUG << "sanityChecks()";
    controller_.sanityChecks();
  }

  /**
   * Fires up the threads by calling start() on the thread group
   */
  void start() {
    LOG_DEBUG << "start()";
    thread_group_.start();
  }

  void shutdown () {
    LOG_DEBUG << "shutdown()";
    thread_group_.stop();
  }

  /**
   * Pushes a message to the ThreadGroup queue
   * @param message
   */
  void pushMessage(DevvMessageUniquePtr message) {
    LOG_DEBUG << "pushMessage()";
    if (message == nullptr) {
      LOG_ERROR << "pushMessage() attempting to push a nullptr";
      throw std::runtime_error("pushMessage() attempting to push a nullptr");
    }
    thread_group_.pushMessage(std::move(message));
  }

 private:
  Controller& controller_;
  ThreadGroup thread_group_;
};

} // namespace Devv
