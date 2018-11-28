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
#include "consensus/blockchain.h"

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

  void push_back(Blockchain::BlockSharedPtr block) {
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

  /**
   * Get the wallet_id associated with this hex_address
   */
  std::string getWallet(const std::string& hex_address);

  /**
   *
   * @param hex_address
   * @param chain_height
   * @param coin
   * @param delta
   * @return
   */
  uint64_t updateBalance(const std::string& hex_address,
                         size_t chain_height,
                         uint64_t coin,
                         int64_t delta);

  /**
   *
   * @param block
   * @param chain_height
   */
  void updateINNTransactions(FinalPtr block, size_t chain_height);

  /**
   *
   * @param block
   * @param chain_height
   */
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

  BlockchainWrapper wrapped_chain_;
};

} // namespace Devv
