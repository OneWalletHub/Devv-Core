/*
 * psql_api.h
 * Classes and helper functions to communicate with postgres
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <string>
#include <mutex>
#include <deque>
#include <condition_variable>

#include <pqxx/pqxx>
#include <pqxx/nontransaction>

#include "primitives/Transaction.h"
#include "primitives/FinalBlock.h"
#include "primitives/blockchain.h"

namespace Devv {

/**
 *
 */
class BlockchainWrapper {
 public:
  explicit BlockchainWrapper(const std::string& name)
  : chain_(name)
  {
  }

  ~BlockchainWrapper() = default;

  void push_back(FinalBlockSharedPtr block) {
    std::lock_guard<std::mutex> guard(mutex_);
    chain_.push_back(block);
  }

  ConstFinalBlockSharedPtr at(size_t loc) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return chain_.at(loc);
  }

  size_t size() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return chain_.size();
  }

  ChainState getHighestChainState() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return chain_.getHighestChainState();
  }

 private:
  mutable std::mutex mutex_;
  Blockchain chain_;
};

class PSQLInterface {
 public:
  typedef std::unique_ptr<pqxx::connection> DBConnectionPtr;
  typedef std::unique_ptr<pqxx::nontransaction> DBTransactionPtr;

  PSQLInterface(const std::string& host,
                const std::string& ip,
                unsigned int port,
                const std::string& name,
                const std::string& user,
                const std::string& pass,
                const std::string& blockchain_name = "psql_interface");

  bool isConnected() const {
    if (db_connection_) {
      return db_connection_->is_open();
    } else {
      return false;
    }
  }

  void handleNextBlock(ConstFinalBlockSharedPtr next_block
                       , size_t block_height);

  void initializeDatabaseConnection();

 private:
  std::string getPendingTransactionID(const Transaction& transaction);

  std::string  hostname_;
  std::string  ip_;
  unsigned int port_;
  std::string  name_;
  std::string  user_;
  std::string  pass_;

  DBConnectionPtr  db_connection_;
  DBTransactionPtr db_transaction_;

  size_t shard_ = 1;

  BlockchainWrapper wrapped_chain_;
};

/**
 * Used to quickly queue incoming messages from the main thread and
 * queue them up to be processed in a worker thread
 */
class ThreadedDBServer {
  typedef std::function<void(ConstFinalBlockSharedPtr, size_t)> CallbackFunction;
  typedef std::function<void()> InitializeCallbackFunction;

 public:
  ThreadedDBServer() = default;

  /**
   * Attach a callback to be executed when a block is ready to be processed
   * @param cb callback function to execute
   */
  void attachCallback(CallbackFunction cb) {
    callback_ = cb;
  }

  void attachInitializeCallback(InitializeCallbackFunction cb) {
    initialize_callback_ = cb;
  }

  /**
   * Start the worker thread
   */
  void startServer();

  /**
   * Stop the worker thread
   */
  void stopServer();

  /**
   * Push a FinalBlock onto the queue. Called from the main thread
   * @param block next block to handle
   */
  void push_queue(ConstFinalBlockSharedPtr block);

 private:
  /**
   * Starts the thread and send loop
   */
  void run() noexcept;

  /// Callback function to execute when a block is ready
  CallbackFunction callback_ = nullptr;

  /// Callback function to execute after thread is created
  InitializeCallbackFunction initialize_callback_ = nullptr;

  /// Current block number
  size_t block_number_ = 0;

  /// Used to run the server service in a background thread
  std::unique_ptr<std::thread> server_thread_ = nullptr;

  /// Set to true when server thread starts and false when a shutdown is requested
  bool keep_running_ = false;

  /// Mutex for synchronization with main thread
  std::mutex cv_mutex_;

  /// Condition variable for wake-up
  std::condition_variable sync_variable_;

  /// Queue of incoming blocks. Main thread pushes, worker thread pops
  std::deque<ConstFinalBlockSharedPtr> input_queue_;

  /// Mutex for input queue
  std::mutex input_mutex_;
};

} // namespace Devv
