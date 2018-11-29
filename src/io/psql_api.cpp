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
const std::string kINSERT_FRESH_TX_STATEMENT = "inaert into fresh_tx (fresh_tx_id, shard_id, block_height, block_time, sig, tx_addr, rx_addr, coin_id, amount, nonce, oracle_name) (select devv_uuid(), $1, $2, $3, $4, $5, $6, $7, $8, $9, $10);";

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

  db_connection_->prepare(kINSERT_FRESH_TX_STATEMENT, kINSERT_FRESH_TX_STATEMENT);
  db_connection_->prepare(kUPDATE_FOR_BLOCK, kUPDATE_FOR_BLOCK_STATEMENT);
}

void PSQLInterface::handleNextBlock(ConstFinalBlockSharedPtr next_block
                                    , size_t block_height) {
  uint64_t blocktime = top_block->getBlockTime();
  std::vector<TransactionPtr> txs = top_block->CopyTransactions();
  pqxx::nontransaction db_context(*db_connection_);
  for (TransactionPtr& one_tx : txs) {
    std::string sig_hex = one_tx->getSignature().getJSON();
    if (one_tx->getSignature().isNodeSignature()) {
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
      }
      stmt.prepared(kINSERT_FRESH_TX)(shard_)(block_height)(blocktime)(sig_hex)(sender_hex)(receiver_hex)(coin_id)(amount)()(oracle_name).exec();
      shard_id, block_height, block_time, sig, tx_addr, rx_addr, coin_id, amount, nonce, oracle_name
    } else {
      //basic transaction
      stmt.prepared(kINSERT_FRESH_TX)(shard_)(block_height)(blocktime)(sig_hex)()()()()()().exec();
	}
  }
}

}
