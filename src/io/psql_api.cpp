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

const std::string kREQUEST_COMMENT = "Test Devv from the INN";

const std::string kNIL_UUID = "00000000-0000-0000-0000-000000000000";
const std::string kNIL_UUID_PSQL = "'00000000-0000-0000-0000-000000000000'::uuid";

const std::string kSELECT_UUID = "select_uuid";
const std::string kSELECT_UUID_STATEMENT = "select devv_uuid();";

const std::string kSELECT_PENDING_TX = "select_pending_tx";
const std::string kSELECT_PENDING_TX_STATEMENT = "select pending_tx_id from pending_tx where sig = upper($1);";

const std::string kSELECT_ADDR = "select_addr";
const std::string kSELECT_ADDR_STATEMENT = "select wallet_addr from wallet where wallet_id = cast($1 as uuid)";

const std::string kSELECT_PENDING_RX = "select_pending_rx";
const std::string kSELECT_PENDING_RX_STATEMENT = "select p.pending_rx_id from pending_rx p where p.sig = upper($1) and p.pending_tx_id = cast($2 as uuid);";

const std::string kSELECT_PENDING_INN = "select_pending_inn";
const std::string kSELECT_PENDING_INN_STATEMENT = "select pending_rx_id, sig, rx_wallet, coin_id, amount from pending_rx where comment = '"+kREQUEST_COMMENT+"' limit $1";

const std::string kSELECT_BALANCE = "balance_select";
const std::string kSELECT_BALANCE_STATEMENT = "select balance from wallet_coin where wallet_id = cast($1 as uuid) and coin_id = $2;";

const std::string kSELECT_WALLET = "wallet_select";
const std::string kSELECT_WALLET_STATEMENT = "select wallet_id from wallet where wallet_addr = upper($1);";

const std::string kSELECT_OLD_PENDING = "select_old_pending";
const std::string kSELECT_OLD_PENDING_STATEMENT = "select pending_tx_id from pending_tx where to_reject = TRUE";

const std::string kSELECT_RECENT_PENDING = "select_recent_pending";
const std::string kSELECT_RECENT_PENDING_STATEMENT = "select pending_tx_id from pending_tx where to_reject = FALSE";

const std::string kINSERT_BALANCE = "balance_insert";
const std::string kINSERT_BALANCE_STATEMENT = "INSERT INTO wallet_coin (wallet_coin_id, wallet_id, block_height, coin_id, balance) (select devv_uuid(), cast($1 as uuid), $2, $3, $4);";

const std::string kUPDATE_BALANCE = "balance_update";
const std::string kUPDATE_BALANCE_STATEMENT = "UPDATE wallet_coin set balance = $1, block_height = $2 where wallet_id = cast($3 as uuid) and coin_id = $4;";

const std::string kSELECT_SHARD = "shard_select";
const std::string kSELECT_SHARD_STATEMENT = "select shard_id from shard where shard_name = $1;";

const std::string kTX_INSERT = "tx_insert";
const std::string kTX_INSERT_STATEMENT = "INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount) (select cast($1 as uuid), $2, $3, $4, tx.wallet_id, $5, $6 from wallet tx where tx.wallet_addr = upper($7));";

const std::string kTX_SELECT = "tx_select";
const std::string kTX_SELECT_STATEMENT = "select tx_id fromk tx where tx_id = cast($1 as uuid);";

const std::string kTX_CONFIRM = "tx_confirm";
const std::string kTX_CONFIRM_STATEMENT = "INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount, comment) (select p.pending_tx_id, $1, $2, $3, p.tx_wallet, p.coin_id, p.amount, p.comment from pending_tx p where p.sig = upper($4) and p.pending_tx_id = cast($5 as uuid));";

const std::string kRX_INSERT = "rx_insert";
const std::string kRX_INSERT_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, tx_id) (select devv_uuid(), $1, $2, $3, tx.wallet_id, rx.wallet_id, $4, $5, $6, cast($7 as uuid) from wallet tx, wallet rx where tx.wallet_addr = upper($8) and rx.wallet_addr = upper($9));";

const std::string kWALLET_INSERT = "wallet_insert";
const std::string kWALLET_INSERT_STATEMENT = "INSERT INTO wallet (wallet_id, wallet_addr, account_id, shard_id, wallet_name) (select cast($1 as uuid), $2, "+kNIL_UUID_PSQL+", 1, 'Unknown')";

const std::string kRX_INSERT_COMMENT = "rx_insert_comment";
const std::string kRX_INSERT_COMMENT_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, comment, tx_id) (select devv_uuid(), $1, $2, $3, tx.wallet_id, rx.wallet_id, $4, $5, $6, $7, cast($8 as uuid) from wallet tx, wallet rx where tx.wallet_addr = upper($9) and rx.wallet_addr = upper($10));";

const std::string kRX_CONFIRM = "rx_confirm";
const std::string kRX_CONFIRM_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, comment, tx_id) (select devv_uuid(), $1, $2, $3, p.tx_wallet, p.rx_wallet, p.coin_id, p.amount, p.delay, p.comment, p.pending_tx_id from pending_rx p where p.pending_rx_id = cast($4 as uuid));";

const std::string kMARK_OLD_PENDING = "mark_old_pending";
const std::string kMARK_OLD_PENDING_STATEMENT = "update pending_tx set to_reject = TRUE where pending_tx_id = cast($1 as uuid);";

const std::string kTX_REJECT = "tx_reject";
const std::string kTX_REJECT_STATEMENT = "INSERT INTO rejected_tx (rejected_tx_id, shard_id, sig, tx_wallet, coin_id, amount, comment) (select p.pending_tx_id, $1, p.sig, p.tx_wallet, p.coin_id, p.amount, p.comment from pending_tx p where p.pending_tx_id = cast($2 as uuid));";

const std::string kDELETE_PENDING_TX = "delete_pending_tx";
const std::string kDELETE_PENDING_TX_STATEMENT = "delete from pending_tx where pending_tx_id = cast($1 as uuid);";

const std::string kDELETE_PENDING_RX = "delete_pending_rx";
const std::string kDELETE_PENDING_RX_STATEMENT = "delete from pending_rx where pending_rx_id = cast($1 as uuid);";

const std::string kDELETE_PENDING_RX_BY_TX = "delete_pending_rx_by_tx";
const std::string kDELETE_PENDING_RX_BY_TX_STATEMENT = "delete from pending_rx where pending_tx_id = cast($1 as uuid);";


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

  db_connection_->prepare(kSELECT_UUID, kSELECT_UUID_STATEMENT);
  db_connection_->prepare(kSELECT_PENDING_TX, kSELECT_PENDING_TX_STATEMENT);
  db_connection_->prepare(kSELECT_PENDING_RX, kSELECT_PENDING_RX_STATEMENT);
  db_connection_->prepare(kSELECT_PENDING_INN, kSELECT_PENDING_INN_STATEMENT);
  db_connection_->prepare(kSELECT_ADDR, kSELECT_ADDR_STATEMENT);
  db_connection_->prepare(kSELECT_OLD_PENDING, kSELECT_OLD_PENDING_STATEMENT);
  db_connection_->prepare(kTX_SELECT, kTX_SELECT_STATEMENT);
  db_connection_->prepare(kTX_INSERT, kTX_INSERT_STATEMENT);
  db_connection_->prepare(kTX_CONFIRM, kTX_CONFIRM_STATEMENT);
  db_connection_->prepare(kRX_INSERT, kRX_INSERT_STATEMENT);
  db_connection_->prepare(kRX_INSERT_COMMENT, kRX_INSERT_COMMENT_STATEMENT);
  db_connection_->prepare(kRX_CONFIRM, kRX_CONFIRM_STATEMENT);
  db_connection_->prepare(kWALLET_INSERT, kWALLET_INSERT_STATEMENT);
  db_connection_->prepare(kSELECT_WALLET, kSELECT_WALLET_STATEMENT);
  db_connection_->prepare(kSELECT_BALANCE, kSELECT_BALANCE_STATEMENT);
  db_connection_->prepare(kINSERT_BALANCE, kINSERT_BALANCE_STATEMENT);
  db_connection_->prepare(kUPDATE_BALANCE, kUPDATE_BALANCE_STATEMENT);
  db_connection_->prepare(kDELETE_PENDING_TX, kDELETE_PENDING_TX_STATEMENT);
  db_connection_->prepare(kDELETE_PENDING_RX, kDELETE_PENDING_RX_STATEMENT);
  db_connection_->prepare(kDELETE_PENDING_RX_BY_TX, kDELETE_PENDING_RX_BY_TX_STATEMENT);
  db_connection_->prepare(kMARK_OLD_PENDING, kMARK_OLD_PENDING_STATEMENT);
  db_connection_->prepare(kSELECT_RECENT_PENDING, kSELECT_RECENT_PENDING_STATEMENT);
  db_connection_->prepare(kTX_REJECT, kTX_REJECT_STATEMENT);

}

std::string PSQLInterface::getWallet(const std::string& hex_address) {
  return std::__cxx11::string();
}

uint64_t PSQLInterface::updateBalance(const std::string& hex_address,
                                      size_t chain_height,
                                      uint64_t coin,
                                      int64_t delta) {
/*
  LOG_INFO << "update_balance(" + hex_addr + ", " +
        std::to_string(coin) + ", " +
        std::to_string(delta) + ")";

  std::string wallet_id;
  Address chain_addr(Hex2Bin(hex_addr));
  int64_t new_balance = state.getAmount(coin, chain_addr);

  try {
    pqxx::result wallet_result = db_transaction_->prepared(kSELECT_WALLET)(hex_addr).exec();
    if (wallet_result.empty()) {
      LOG_INFO << "wallet_result.empty()";
      pqxx::result uuid_result = db_transaction_->prepared(kSELECT_UUID).exec();
      if (!uuid_result.empty()) {
        LOG_INFO << "!uuid_result.empty():";
        wallet_id = uuid_result[0][0].as<std::string>();
        LOG_INFO << "UUID is: " + wallet_id;
        db_transaction_->prepared(kWALLET_INSERT)(wallet_id)(hex_addr).exec();
      } else {
        LOG_WARNING << "Failed to generate a UUID for new wallet!";
        return 0;
      }
    } else {
      wallet_id = wallet_result[0][0].as<std::string>();
      LOG_INFO << "Got wallet ID: " + wallet_id;
    }
  } catch (const pqxx::pqxx_exception& e) {
    LOG_ERROR << e.base().what() << std::endl;
    auto s = dynamic_cast<const pqxx::sql_error*>(&e.base());
    if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
    throw;
  } catch (const std::exception& e) {
    LOG_WARNING << FormatException(&e, "Exception selecting wallet");
    throw;
  } catch (...) {
    LOG_WARNING << "caught (...)";
    throw;
  }


  try {
    pqxx::result balance_result = db_transaction_->prepared(kSELECT_BALANCE)(wallet_id)(coin).exec();
    if (balance_result.empty()) {
      LOG_INFO << "No balance, insert wallet_coin";

      db_transaction_->prepared(kINSERT_BALANCE)(wallet_id)(chain_height)(coin)(new_balance).exec();
    } else {
      LOG_INFO << "New balance is: " + std::to_string(new_balance);
      db_transaction_->prepared(kUPDATE_BALANCE)(new_balance)(chain_height)(wallet_id)(coin).exec();
    }
  } catch (const pqxx::pqxx_exception& e) {
    LOG_ERROR << e.base().what() << std::endl;
    auto s = dynamic_cast<const pqxx::sql_error*>(&e.base());
    if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
    throw;
  } catch (const std::exception& e) {
    LOG_WARNING << FormatException(&e, "Exception selecting balance");
    throw;
  } catch (...) {
    LOG_WARNING << "caught (...)";
    throw;
  }

  LOG_INFO << "balance updated to: "+std::to_string(new_balance);
  return new_balance;
  */
  return 0;
}

void PSQLInterface::updateINNTransactions(FinalPtr block, size_t chain_height) {

  auto blocktime = block->getBlockTime();
  auto transactions = block->CopyTransactions();

  size_t num_inn_tx = 0;
  for (TransactionPtr& one_tx : transactions) {
    std::string sig_hex = one_tx->getSignature().getJSON();
    if (one_tx->getSignature().isNodeSignature()) {
      num_inn_tx++;
    }
  }

  // Only continue if we have INN transactions
  if (num_inn_tx < 1) return;

  //std::lock_guard<std::mutex> guard(inn_mutex_);
  pqxx::result inn_result = db_transaction_->prepared(kSELECT_PENDING_INN)(num_inn_tx).exec();
  LOG_DEBUG << "SELECT_PENDING_INN returned (" << inn_result.size() << ") transactions";
  for (auto result : inn_result) {
    auto pending_uuid = result[0].as<std::string>();
    auto uuid = result[1].as<std::string>();
    auto rx_wallet = result[2].as<std::string>();
    LOG_INFO << "Handle inn transaction for: " + rx_wallet;
    auto coin = result[3].as<uint64_t>();
    auto amount = result[4].as<int64_t>();
    auto addr_result = db_transaction_->prepared(kSELECT_ADDR)(rx_wallet).exec();

    if (addr_result.empty()) {
      LOG_WARNING << "wallet id '" + rx_wallet + "' has no address?";
      continue;
    }
    if (amount < 0) {
      LOG_WARNING << "Negative coin request. Ignore.";
      continue;
    }
    std::string rx_addr = addr_result[0][0].as<std::string>();
    db_transaction_->prepared(kTX_INSERT)(uuid)(shard_)(chain_height)(blocktime)(coin)(-1 * amount)(kNIL_UUID).exec();
    //update_balance(stmt, rx_addr, chain_height, coin, amount, shard_, state);
    LOG_INFO << "Receiver balance updated.";
    uint64_t delay = 0;
    db_transaction_->prepared(kRX_INSERT_COMMENT)(shard_)(chain_height)(blocktime)(coin)(amount)(delay)(kREQUEST_COMMENT)(uuid)(
        kNIL_UUID)(rx_addr).exec();
    LOG_INFO << "Updated rx table";
    db_transaction_->prepared(kDELETE_PENDING_RX)(pending_uuid).exec();
    db_transaction_->prepared(kDELETE_PENDING_TX)(uuid).exec();
    db_transaction_->exec("commit;");
  }
}

void PSQLInterface::updateWalletTransactions(FinalPtr block, size_t chain_height) {
/*
 * auto blocktime = block->getBlockTime();
  auto transactions = block->CopyTransactions();

  /// updateINNTransactions(block, height);

  for (TransactionPtr& one_tx : transactions) {
    LOG_INFO << "Begin processing transaction.";

    // Skip INN transactions
    if (one_tx->getSignature().isNodeSignature()) {
      continue;
    }

    auto sig_hex = one_tx->getSignature().getJSON();
    auto transfers = one_tx->getTransfers();

    std::string sender_hex;
    uint64_t coin_id = 0;
    int64_t send_amount = 0;

    for (TransferPtr& one_xfer : transfers) {
      if (one_xfer->getAmount() < 0) {
        if (!sender_hex.empty()) {
          LOG_WARNING << "Multiple senders in transaction '" + sig_hex + "'?!";
        }
        sender_hex = one_xfer->getAddress().getHexString();
        coin_id = one_xfer->getCoin();
        send_amount = one_xfer->getAmount();
        break;
      }
    } // end sender search loop

    //update_balance(stmt, sender_hex, chain_height, coin_id, send_amount, options->shard_index, state);

    // copy transfers
    pqxx::result pending_result;
    try {
      pending_result = db_transaction_->prepared(kSELECT_PENDING_TX)(sig_hex).exec();
      LOG_DEBUG << LOG_RESULT(pending_result);
    } catch (const pqxx::pqxx_exception& e) {
      LOG_ERROR << e.base().what() << std::endl;
      auto s = dynamic_cast<const pqxx::sql_error*>(&e.base());
      if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
    } catch (const std::exception& e) {
      LOG_WARNING << FormatException(&e, "Exception updating database for transfer, no rollback: " + sig_hex);
    } catch (...) {
      LOG_WARNING << "caught (...)";
    }


  }
  */
}

bool PSQLInterface::deletePendingTx(const std::string& pending_tx_id) {
  return false;
}

bool PSQLInterface::deletePendingRx(const std::string& pending_rx_id) {
  return false;
}

bool PSQLInterface::insertTx() {
  return false;
}

bool PSQLInterface::insertRx() {
  return false;
}

std::string PSQLInterface::getPendingTransactionID(const Transaction& transaction) {
  return std::__cxx11::string();
}

}
