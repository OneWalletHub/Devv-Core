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
  auto blocktime = next_block->getBlockTime();
  const std::vector<TransactionPtr>& transactions = next_block->getTransactions();

  LOG_DEBUG << "handleNextBlock(), height: " << block_height << " txs: " << transactions.size();
  if (db_connection_ == nullptr) {
    LOG_ERROR << "Database not initialized - skipping block!";
    return;
  }

  pqxx::nontransaction db_context(*db_connection_);
  for (auto& one_tx : transactions) {
    auto sig = one_tx->getSignature();
    auto sig_hex = sig.getJSON();
    if (sig.isNodeSignature()) {
      //coin request transaction in this version
      std::vector<TransferPtr> xfers = one_tx->getTransfers();
      std::string sender_hex;
      std::string receiver_hex;
      uint64_t coin_id = 0;
      int64_t amount = 0;
      std::string oracle_name = "io.devv.coin_request";

      for (TransferPtr& one_xfer : xfers) {
        if (one_xfer->getAmount() < 0) {
          sender_hex = one_xfer->getAddress().getHexString();
        } else {
          receiver_hex = one_xfer->getAddress().getHexString();
          coin_id = one_xfer->getCoin();
          amount = one_xfer->getAmount();
        }
        LOG_DEBUG << "coin_request: shard(" << std::to_string(shard_) << ")"
                  << " block_height(" << std::to_string(block_height) << ")"
                  << " block_time(" << std::to_string(blocktime) << ")"
                  << " sig_hex(" << sig_hex << ")"
                  << " tx_addr(" << sender_hex << ")"
                  << " rx_addr(" << receiver_hex << ")"
                  << " coin_id(" << std::to_string(coin_id) << ")"
                  << " amount(" << std::to_string(amount) << ")"
                  << " nonce()"
                  << " oracle_name(" << oracle_name << ")";
      }
      db_context.prepared(kINSERT_FRESH_TX)(shard_)(block_height)(blocktime)(sig_hex)(sender_hex)(receiver_hex)(coin_id)(amount)()(oracle_name).exec();
    } else {
      //basic transaction
      db_context.prepared(kINSERT_FRESH_TX)(shard_)(block_height)(blocktime)(sig_hex)()()()()()().exec();
        LOG_DEBUG << "coin_request: shard(" << std::to_string(shard_) << ")"
                  << " block_height(" << std::to_string(block_height) << ")"
                  << " block_time(" << std::to_string(blocktime) << ")"
                  << " sig_hex(" << sig_hex << ")";
    }
  }
  db_context.prepared(kUPDATE_FOR_BLOCK)(block_height).exec();
  db_context.exec("commit;");
}

void ThreadedDBServer::startServer() {
  LOG_DEBUG << "Starting DBServer";
  if (keep_running_) {
    LOG_WARNING << "Attempted to start a ThreadedDBServer that was already running";
    return;
  }
  keep_running_ = true;
  server_thread_ = std::make_unique<std::thread>([this]() { this->run(); });
}

void ThreadedDBServer::stopServer() {
  LOG_DEBUG << "Stopping ThreadedDBServer";
  if (keep_running_) {
    keep_running_ = false;
    server_thread_->join();
    LOG_INFO << "Stopped ThreadedDBServer";
  } else {
    LOG_WARNING << "Attempted to stop a stopped server!";
  }
}

void ThreadedDBServer::push_queue(Devv::ConstFinalBlockSharedPtr block) {
  LOG_DEBUG << "push_queue(): waiting on lock";
  std::lock_guard<std::mutex> guard(input_mutex_);
  input_queue_.push_back(block);

  std::unique_lock<std::mutex> lk(cv_mutex_);
  sync_variable_.notify_one();
  LOG_DEBUG << "push_queue(): sync_variable_.notify_one()";
}

void ThreadedDBServer::run() noexcept {

  LOG_DEBUG << "run(): calling initialize_callback";
  initialize_callback_();

  // Run forever (break on shutdown)
  do {
    std::unique_lock<std::mutex> lk(cv_mutex_);
    LOG_DEBUG << "run(): Waiting for notification";
    sync_variable_.wait(lk, [&] {
                          std::unique_lock<std::mutex> input_lock;
                          return !input_queue_.empty();
                        }
    );

    // Create a local deque and empty the shared deque into
    // the local copy quickly and release the lock
    std::deque<ConstFinalBlockSharedPtr> tmp_vec;

    LOG_DEBUG << "run(): creating tmp vector";
    {
      std::unique_lock<std::mutex> input_lock;
      for (auto block : input_queue_) {
        tmp_vec.push_back(block);
      }
      input_queue_.clear();
    }
    LOG_DEBUG << "run(): tmp_vec created, size: " << tmp_vec.size();

    // Now that the lock is released, loop through
    // the temp vector and handle blocks
    for (auto block : tmp_vec) {
      callback_(block, block_number_);
      ++block_number_;
    }
  } while (keep_running_);
}

} // namespace Devv
