/*
 * psql_api.h
 * Classes and helper functions to communicate with postgres
 *
 * @copywrite  2018 Devvio Inc
 */
#include "io/psql_api.h"

#include <exception>

#include "common/logger.h"

namespace Devv {

#define LOG_RESULT(res) res.size() << " : " << res.query()

const std::string kINSERT_FRESH_TX = "insert_fresh_tx";
const std::string kINSERT_FRESH_TX_STATEMENT = "insert into fresh_tx (fresh_tx_id, shard_id, block_height, block_time, sig, tx_addr, rx_addr, coin_id, amount, nonce, oracle_name) (select devv_uuid(), $1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";

const std::string kREJECT_OLD_TX = "reject_old_tx";
const std::string kREJECT_OLD_TX_STATEMENT = "select reject_old_txs();";

const std::string kUPDATE_FOR_BLOCK = "update_for_block";
const std::string kUPDATE_FOR_BLOCK_STATEMENT = "select update_for_block($1);";

PSQLInterface::PSQLInterface(const std::string& host,
                             const std::string& ip,
                             unsigned int port,
                             const std::string& name,
                             const std::string& user,
                             const std::string& pass,
                             const std::string& blockchain_name)
  : hostname_(host),
    ip_(ip),
    port_(port),
    name_(name),
    user_(user),
    pass_(pass),
    wrapped_chain_(blockchain_name)
{
}

void PSQLInterface::initializeDatabaseConnection() {
  if (!(hostname_.empty() || ip_.empty())) {
    throw std::runtime_error("Database hostname or IP is required!");
  }

  if (name_.empty()) {
    throw std::runtime_error("Database name required!");
  }

  if (user_.empty()) {
    throw std::runtime_error("Database user required!");
  }

  if (pass_.empty()) {
    throw std::runtime_error("Database password required!");
  }

  std::string params("dbname = " + name_ +
      " user = " + user_ +
      " password = " + pass_);
  if (!hostname_.empty()) {
    params += " host = " + hostname_;
  } else if (!ip_.empty()) {
    params += " hostaddr = " + ip_;
  }

  params += " port = "+std::to_string(port_);
  LOG_INFO << "Using db connection params: "+params;

  db_connection_ = std::make_unique<pqxx::connection>(params);
  LOG_INFO << "Successfully connected to database.";

  db_connection_->prepare(kINSERT_FRESH_TX, kINSERT_FRESH_TX_STATEMENT);
  db_connection_->prepare(kREJECT_OLD_TX, kREJECT_OLD_TX_STATEMENT);
  db_connection_->prepare(kUPDATE_FOR_BLOCK, kUPDATE_FOR_BLOCK_STATEMENT);
}

void PSQLInterface::handleNextBlock(ConstFinalBlockSharedPtr next_block
                                    , size_t block_height) {
}

void ThreadedDBServer::startServer() {
  LOG_DEBUG << "Starting DBServer";
  if (keep_running_) {
    LOG_WARNING << "Attempted to start a ThreadedDBServer that was already running";
    return;
  }
  server_thread_ = std::make_unique<std::thread>([this]() { this->run(); });
  keep_running_ = true;
}

void ThreadedDBServer::stopServer() {
  LOG_DEBUG << "Stopping TransactionServer";
  if (keep_running_) {
    keep_running_ = false;
    server_thread_->join();
    LOG_INFO << "Stopped TransactionServer";
  } else {
    LOG_WARNING << "Attempted to stop a stopped server!";
  }
}

void ThreadedDBServer::push_queue(Devv::ConstFinalBlockSharedPtr block) {
  std::lock_guard<std::mutex> guard(input_mutex_);
  input_queue_.push_back(block);
}

void ThreadedDBServer::run() noexcept {
  std::unique_lock<std::mutex> lk(input_mutex_);
  std::cerr << "Waiting... \n";
  sync_variable_.wait(lk, [&] {
                        std::unique_lock<std::mutex> input_lock;
                        return !input_queue_.empty();
                      }
  );

  // Create a local deque and empty the shared deque into
  // the local copy quickly and release the lock
  std::deque<ConstFinalBlockSharedPtr> tmp_vec;

  {
    std::unique_lock<std::mutex> input_lock;
    for (auto block : input_queue_) {
      tmp_vec.push_back(block);
    }
    input_queue_.clear();
  }

  // Now that the lock is released, loop through
  // the temp vector and handle blocks
  for (auto block : tmp_vec) {
    callback_(block, block_number_);
    ++block_number_;
  }
}

} // namespace Devv
