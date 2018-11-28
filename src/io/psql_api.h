/*
 * psql_api.h
 * Classes and helper functions to communicate with postgres
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <string>
#include <mutex>

#include <pqxx/pqxx>
#include <pqxx/nontransaction>

#include "primitives/Transaction.h"
#include "primitives/FinalBlock.h"

namespace Devv {

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
                std::mutex& inn_mutex);

  bool isConnected() const {
    if (db_connection_) {
      return db_connection_->is_open();
    } else {
      return false;
    }
  }

  /**
   * Get the wallet_id associated with this hex_address
   */
  std::string getWallet(const std::string& hex_address);

  uint64_t updateBalance(const std::string& hex_address,
                         size_t chain_height,
                         uint64_t coin,
                         int64_t delta);

  void updateINNTransactions(FinalPtr block, size_t chain_height);

  void updateWalletTransactions(FinalPtr block, size_t chain_height);

  bool deletePendingTx(const std::string& pending_tx_id);

  bool deletePendingRx(const std::string& pending_rx_id);

  bool insertTx();

  bool insertRx();

 private:
  void initializeDatabaseConnection();

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

  std::mutex& inn_mutex_;
};

} // namespace Devv
