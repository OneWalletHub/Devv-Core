
/*
 * devv-psql.cpp listens for FinalBlock messages and updates an INN database
 * based on the transactions in those blocks.
 * This process is designed to integrate with a postgres database.
 *
 *  Created on: September 6, 2018
 *  Author: Nick Williams
 */

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <pqxx/pqxx>
#include <pqxx/nontransaction>

#include "common/logger.h"
#include "common/devv_context.h"
#include "io/message_service.h"
#include "modules/BlockchainModule.h"

using namespace Devv;

namespace fs = boost::filesystem;

#define DEBUG_TRANSACTION_INDEX (processed + 11000000)
typedef std::chrono::milliseconds millisecs;

/**
 * Holds command-line options
 */
struct psql_options {
  std::vector<std::string> host_vector{};
  eAppMode mode = eAppMode::T1;
  unsigned int shard_index = 0;
  std::string stop_file;
  eDebugMode debug_mode = eDebugMode::off;

  std::string db_user;
  std::string db_pass;
  std::string db_host;
  std::string db_ip;
  std::string db_name;
  unsigned int db_port = 5432;
};

static const std::string kNIL_UUID = "00000000-0000-0000-0000-000000000000";
static const std::string kNIL_UUID_PSQL = "'00000000-0000-0000-0000-000000000000'::uuid";
static const std::string kSELECT_UUID = "select_uuid";
static const std::string kSELECT_UUID_STATEMENT = "select devv_uuid();";
static const std::string kSELECT_PENDING_TX = "select_pending_tx";
static const std::string kSELECT_PENDING_TX_STATEMENT = "select pending_tx_id from pending_tx where sig = lower($1);";
static const std::string kSELECT_PENDING_RX = "select_pending_rx";
static const std::string kSELECT_PENDING_RX_STATEMENT = "select p.pending_rx_id from pending_rx p, wallet rx where p.sig = lower($1) and rx.wallet_id = cast($2 as uuid);";
static const std::string kTX_INSERT = "tx_insert";
static const std::string kTX_INSERT_STATEMENT = "INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount) (select cast($1 as uuid), $2, $3, $4, tx.wallet_id, $5, $6 from wallet tx where tx.wallet_addr = lower($7));";
static const std::string kTX_CONFIRM = "tx_confirm";
static const std::string kTX_CONFIRM_STATEMENT = "INSERT INTO tx (tx_id, shard_id, block_height, block_time, tx_wallet, coin_id, amount, comment) (select p.pending_tx_id, $1, $2, $3, p.tx_wallet, p.coin_id, p.amount, p.comment from pending_tx p where tx.wallet_addr = lower($4) and p.pending_tx_id = cast($5 as uuid));";
static const std::string kRX_INSERT = "rx_insert";
static const std::string kRX_INSERT_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, tx_id) (select devv_uuid(), $1, $2, $3, tx.wallet_id, rx.wallet_id, $4, $5, $6, cast($7 as uuid) from wallet tx, wallet rx where tx.wallet_addr = lower($8) and rx.wallet_addr = lower($9));";
static const std::string kRX_CONFIRM = "rx_confirm";
static const std::string kRX_CONFIRM_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, block_time, tx_wallet, rx_wallet, coin_id, amount, delay, comment, tx_id) (select devv_uuid(), $1, $2, $3, p.tx_wallet, p.rx_wallet, p.coin_id, p.amount, p.delay, p.comment, p.pending_tx_id from pending_rx p where p.pending_rx_id = cast($4 as uuid));";
static const std::string kBALANCE_SELECT = "balance_select";
static const std::string kBALANCE_SELECT_STATEMENT = "select balance from wallet_coin where wallet_id = cast($1 as uuid) and coin_id = $2;";
static const std::string kWALLET_SELECT = "wallet_select";
static const std::string kWALLET_SELECT_STATEMENT = "select wallet_id from wallet where wallet_addr = lower($1);";
static const std::string kWALLET_INSERT = "wallet_insert";
static const std::string kWALLET_INSERT_STATEMENT = "INSERT INTO wallet (wallet_id, wallet_addr, account_id, wallet_name, shard_id) (select cast($1 as uuid), lower($2), cast('"+kNIL_UUID+"' as uuid), 'None', $3);";
static const std::string kBALANCE_INSERT = "balance_insert";
static const std::string kBALANCE_INSERT_STATEMENT = "INSERT INTO wallet_coin (wallet_coin_id, wallet_id, block_height, coin_id, balance) (select devv_uuid(), cast($1 as uuid), $2, $3, $4);";
static const std::string kBALANCE_UPDATE = "balance_update";
static const std::string kBALANCE_UPDATE_STATEMENT = "UPDATE wallet_coin set balance = $1, block_height = $2 where wallet_id = cast($3 as uuid) and coin_id = $4;";
static const std::string kSHARD_SELECT = "shard_select";
static const std::string kSHARD_SELECT_STATEMENT = "select shard_id from shard where shard_name = $1;";
static const std::string kDELETE_PENDING_TX = "delete_pending_tx";
static const std::string kDELETE_PENDING_TX_STATEMENT = "delete from pending_tx_id where pending_tx_id = cast($1 as uuid);";
static const std::string kDELETE_PENDING_RX = "delete_pending_rx";
static const std::string kDELETE_PENDING_RX_STATEMENT = "delete from pending_rx_id where pending_rx_id = cast($1 as uuid);";

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct psql_options> ParsePsqlOptions(int argc, char** argv);

/**
 * Update wallet balance
 * @param stmt a nontransaction statement to use to communicate with the db
 * @param hex_addr - the wallet address in hex
 * @param coin - the index of the currency to update
 * @param delta - the positive or negative amount to change the balance
 * @note will update balances to negative numbers (allowed during coin creation)
 * @note this method is not atomic and may be part of larger sql transactions
 * @return the new balance as int64_t
 */
int64_t update_balance(pqxx::nontransaction& stmt, std::string hex_addr
        , unsigned int chain_height, uint64_t coin, int64_t delta, int shard) {
  LOG_INFO << "update_balance("+hex_addr+", "+std::to_string(coin)+", "+std::to_string(delta)+")";
  std::string wallet_id = "";
  int64_t new_balance = delta;
  LOG_INFO << "Get wallet: "+hex_addr;
  try {
    pqxx::result wallet_result = stmt.prepared(kWALLET_SELECT)(hex_addr).exec();
    if (wallet_result.empty()) {
      LOG_INFO << "No result";
      pqxx::result uuid_result = stmt.prepared(kSELECT_UUID).exec();
      if (!uuid_result.empty()) {
        LOG_INFO << "Got uuid";
        wallet_id = uuid_result[0][0].as<std::string>();
        LOG_INFO << "UUID is: " + wallet_id;
        try {
          stmt.prepared(kWALLET_INSERT)(wallet_id)(hex_addr)(shard).exec();
        } catch (const pqxx::pqxx_exception& e) {
          std::cerr << e.base().what() << std::endl;
          const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e.base());
          if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
        } catch (const std::exception& e) {
          LOG_WARNING << FormatException(&e, "Exception inserting new wallet");
        }
        LOG_INFO << "Inserted wallet.";
      } else {
        LOG_WARNING << "Failed to generate a UUID for new wallet!";
        return 0;
      }
    } else {
      wallet_id = wallet_result[0][0].as<std::string>();
      LOG_INFO << "Got wallet ID: " + wallet_id;
    }
  } catch (const pqxx::pqxx_exception& e) {
    std::cerr << e.base().what() << std::endl;
    const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e.base());
    if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
  } catch (const std::exception& e) {
    LOG_WARNING << FormatException(&e, "Exception selecting wallet");
  }

  try {
    pqxx::result balance_result = stmt.prepared(kBALANCE_SELECT)(wallet_id)(coin).exec();
    if (balance_result.empty()) {
      LOG_INFO << "No balance, insert wallet_coin";
      stmt.prepared(kBALANCE_INSERT)(wallet_id)(chain_height)(coin)(delta).exec();
    } else {
      new_balance = balance_result[0][0].as<int64_t>() + delta;
      LOG_INFO << "New balance is: " + std::to_string(new_balance);
      stmt.prepared(kBALANCE_UPDATE)(new_balance)(chain_height)(wallet_id)(coin).exec();
    }
  } catch (const pqxx::pqxx_exception& e) {
    std::cerr << e.base().what() << std::endl;
    const pqxx::sql_error* s = dynamic_cast<const pqxx::sql_error*>(&e.base());
    if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
  } catch (const std::exception& e) {
    LOG_WARNING << FormatException(&e, "Exception selecting balance");
  }

  LOG_INFO << "balance updated to: "+std::to_string(new_balance);
  return new_balance;
}

int main(int argc, char* argv[]) {
  init_log();

  try {
    auto options = ParsePsqlOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    zmq::context_t zmq_context(1);

    MTR_SCOPE_FUNC();

    std::string shard_name = "Shard-"+std::to_string(options->shard_index);

    bool db_connected = false;
    std::unique_ptr<pqxx::connection> db_link = nullptr;
    if ((!options->db_host.empty() || !options->db_ip.empty()) && !options->db_user.empty()) {
      std::string db_params("dbname = "+options->db_name +
          " user = "+options->db_user+
          " password = "+options->db_pass);
      if (!options->db_host.empty()) {
        db_params += " host = "+options->db_host;
      } else if (!options->db_ip.empty()) {
        db_params += " hostaddr = "+options->db_ip;
      } else {
        LOG_FATAL << "Database hostname or IP is required!";
        throw std::runtime_error("Database hostname or IP is required!");
      }
      db_params += " port = "+std::to_string(options->db_port);
      LOG_NOTICE << "Using db connection params: "+db_params;
      try {
        //throws an exception if the connection failes
        db_link = std::make_unique<pqxx::connection>(db_params);
      } catch (const std::exception& e) {
        LOG_FATAL << FormatException(&e, "Database connection failed: "
          + db_params);
        throw;
      }
      //comments in pqxx say to resist the urge to immediately call
      //db_link->is_open(), if there was no exception above the
      //connection should be established
      LOG_INFO << "Successfully connected to database.";
      db_connected = true;
      db_link->prepare(kSELECT_UUID, kSELECT_UUID_STATEMENT);
      db_link->prepare(kSELECT_PENDING_TX, kSELECT_PENDING_TX_STATEMENT);
      db_link->prepare(kSELECT_PENDING_RX, kSELECT_PENDING_RX_STATEMENT);
      db_link->prepare(kBALANCE_UPDATE, kBALANCE_UPDATE_STATEMENT);
      db_link->prepare(kTX_INSERT, kTX_INSERT_STATEMENT);
      db_link->prepare(kTX_CONFIRM, kTX_CONFIRM_STATEMENT);
      db_link->prepare(kRX_INSERT, kRX_INSERT_STATEMENT);
      db_link->prepare(kRX_CONFIRM, kRX_CONFIRM_STATEMENT);
      db_link->prepare(kWALLET_SELECT, kWALLET_SELECT_STATEMENT);
      db_link->prepare(kWALLET_INSERT, kWALLET_INSERT_STATEMENT);
      db_link->prepare(kBALANCE_SELECT, kBALANCE_SELECT_STATEMENT);
      db_link->prepare(kBALANCE_INSERT, kBALANCE_INSERT_STATEMENT);
      db_link->prepare(kBALANCE_UPDATE, kBALANCE_UPDATE_STATEMENT);
      db_link->prepare(kDELETE_PENDING_TX, kDELETE_PENDING_TX_STATEMENT);
      db_link->prepare(kDELETE_PENDING_RX, kDELETE_PENDING_RX_STATEMENT);
    } else {
      LOG_FATAL << "Database host and user not set!";
    }

    //@todo(nick@cloudsolar.co): read pre-existing chain
    unsigned int chain_height = 0;

    auto peer_listener = io::CreateTransactionClient(options->host_vector, zmq_context);
    peer_listener->attachCallback([&](DevvMessageUniquePtr p) {
      if (p->message_type == eMessageType::FINAL_BLOCK) {
        //update database
        if (db_connected) {
          InputBuffer buffer(p->data);
          ChainState priori;
          KeyRing keys;
          FinalBlock one_block(buffer, priori, keys, options->mode);
          uint64_t blocktime = one_block.getBlockTime();
          std::vector<TransactionPtr> txs = one_block.CopyTransactions();

          for (TransactionPtr& one_tx : txs) {
            pqxx::nontransaction stmt(*db_link);
            LOG_INFO << "Begin processing transaction.";
            stmt.exec("begin;");
            stmt.exec("savepoint tx_savepoint;");
            std::string sig_hex = one_tx->getSignature().getJSON();
            try {
              std::vector<TransferPtr> xfers = one_tx->getTransfers();
              std::string sender_hex;
              uint64_t coin_id = 0;
              int64_t send_amount = 0;

              for (TransferPtr& one_xfer : xfers) {
                if (one_xfer->getAmount() < 0) {
                  if (!sender_hex.empty()) {
                    LOG_WARNING << "Multiple senders in transaction '"+sig_hex+"'?!";
                  }
                  sender_hex = one_xfer->getAddress().getHexString();
                  coin_id = one_xfer->getCoin();
                  send_amount = one_xfer->getAmount();
                  break;
                }
              } //end sender search loop

              LOG_INFO << "Update sender balance.";
              update_balance(stmt, sender_hex, chain_height, coin_id, send_amount, options->shard_index);
              LOG_INFO << "Updated sender balance.";

              //copy transfers
              pqxx::result pending_result;
              try {
                pending_result = stmt.prepared(kSELECT_PENDING_TX)(sig_hex).exec();
              } catch (const pqxx::pqxx_exception &e) {
                std::cerr << e.base().what() << std::endl;
                const pqxx::sql_error *s=dynamic_cast<const pqxx::sql_error*>(&e.base());
                if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
              } catch (const std::exception& e) {
                LOG_WARNING << FormatException(&e, "Exception updating database for transfer, no rollback: "+sig_hex);
              }

              if (!pending_result.empty()) {
                LOG_INFO << "Pending transaction exists.";
                std::string pending_uuid = pending_result[0][0].as<std::string>();
                stmt.prepared(kTX_CONFIRM)(options->shard_index)(chain_height)(blocktime)(sender_hex)(pending_uuid).exec();
                stmt.exec("commit;");
                for (TransferPtr& one_xfer : xfers) {
                  int64_t rcv_amount = one_xfer->getAmount();
                  if (rcv_amount < 0) continue;
                  std::string rcv_addr = one_xfer->getAddress().getHexString();
                  uint64_t rcv_coin_id = one_xfer->getCoin();
                  try {
                    LOG_INFO << "Begin processing transfer.";
                    stmt.exec("begin;");
                    stmt.exec("savepoint rx_savepoint;");
                    LOG_INFO << "Update receiver balance.";
                    update_balance(stmt, rcv_addr, chain_height, rcv_coin_id, rcv_amount, options->shard_index);
                    pqxx::result rx_result = stmt.prepared(kSELECT_PENDING_RX)(sig_hex)(rcv_addr).exec();
                    if (!rx_result.empty()) {
                      std::string rx_uuid = rx_result[0][0].as<std::string>();
                      LOG_INFO << "Insert rx row.";
                      stmt.prepared(kRX_CONFIRM)(options->shard_index)(chain_height)(blocktime)(rx_uuid).exec();
                      LOG_INFO << "Delete pending rx.";
                      stmt.prepared(kDELETE_PENDING_RX)(rx_uuid).exec();
                    } else {
                      LOG_WARNING << "Pending tx missing corresponding rx '"+sig_hex+"'!";
                    }
                    stmt.exec("commit;");
                    LOG_INFO << "Transfer committed.";
                  } catch (const pqxx::pqxx_exception& e) {
                    std::cerr << e.base().what() << std::endl;
                  } catch (const std::exception& e) {
                    LOG_WARNING << FormatException(&e, "Exception updating database for transfer, rollback: "+sig_hex);
                    stmt.exec("rollback to savepoint rx_savepoint;");
                  }
                } //end rx copy loop
                LOG_INFO << "Delete pending transaction.";
                stmt.exec("begin;");
                stmt.exec("savepoint delete_savepoint;");
                stmt.prepared(kDELETE_PENDING_TX)(pending_uuid).exec();
                stmt.exec("commit;");
              } else { //no pending exists, so create new transaction
                pqxx::result uuid_result = stmt.prepared(kSELECT_UUID).exec();
                if (!uuid_result.empty()) {
                  std::string tx_uuid = uuid_result[0][0].as<std::string>();
                  stmt.prepared(kTX_INSERT)(tx_uuid)(options->shard_index)(chain_height)(blocktime)(coin_id)(send_amount)(sender_hex).exec();
                  stmt.exec("commit;");
                  LOG_INFO << "Pending transaction does not exist.";
                  for (TransferPtr& one_xfer : xfers) {
                    //only do receivers
                    int64_t rcv_amount = one_xfer->getAmount();
                    if (rcv_amount < 0) continue;
                    try {
                      LOG_INFO << "Begin processing transfer.";
                      stmt.exec("begin;");
                      stmt.exec("savepoint rx_savepoint;");
                      std::string rcv_addr = one_xfer->getAddress().getHexString();
                      uint64_t rcv_coin_id = one_xfer->getCoin();
                      uint64_t delay = one_xfer->getDelay();

                      //update receiver balance
                      LOG_INFO << "Update receiver balance.";
                      try {
                        update_balance(stmt, rcv_addr, chain_height, rcv_coin_id, rcv_amount, options->shard_index);
                      } catch (const pqxx::pqxx_exception& e) {
                        std::cerr << e.base().what() << std::endl;
                      } catch (const std::exception& e) {
                        LOG_WARNING << FormatException(&e, "Exception updating balance");
                      }
                      LOG_INFO << "Insert rx row.";
                      stmt.prepared(kRX_INSERT)(options->shard_index)(chain_height)(blocktime)(rcv_coin_id)(rcv_amount)(delay)(tx_uuid)(sender_hex)(rcv_addr).exec();

                      stmt.exec("commit;");
                      LOG_INFO << "Transfer committed.";
                    } catch (const std::exception& e) {
                      LOG_WARNING << FormatException(&e, "Exception updating database for transfer, no rollback: "+sig_hex);
                    }
                  } //end transfer loop
                } else {
                  LOG_WARNING << "Failed to generate a UUID!";
                }
              }
            } catch (const pqxx::pqxx_exception &e) {
              std::cerr << e.base().what() << std::endl;
              const pqxx::sql_error *s=dynamic_cast<const pqxx::sql_error*>(&e.base());
              if (s) LOG_ERROR << "Query was: " << s->query() << std::endl;
            } catch (const std::exception& e) {
              LOG_WARNING << FormatException(&e, "Exception updating database for transfer, no rollback: "+sig_hex);
            }
            LOG_DEBUG << "Finished processing transaction.";
          } //end transaction loop
        } else { //endif db connected?
          throw std::runtime_error("Database is not connected!");
        }
        chain_height++;
        LOG_INFO << "Ready for block: "+std::to_string(chain_height);
      }
    });
    peer_listener->listenTo(get_shard_uri(options->shard_index));
    peer_listener->startClient();
    LOG_INFO << "devv-psql is listening to shard: "+get_shard_uri(options->shard_index);

    auto ms = kMAIN_WAIT_INTERVAL;
    while (true) {
      LOG_DEBUG << "devv-psql sleeping for " << ms;
      std::this_thread::sleep_for(millisecs(ms));
      /* Should we shutdown? */
      if (fs::exists(options->stop_file)) {
        LOG_INFO << "Shutdown file exists. Stopping devv-psql...";
        break;
      }
    }
    if (db_connected) db_link->disconnect();
    peer_listener->stopClient();
    return (true);
  }
  catch (const std::exception& e) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: " + err;
    return (false);
  }
}


std::unique_ptr<struct psql_options> ParsePsqlOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<psql_options> options(new psql_options());
  std::vector<std::string> config_filenames;

  try {
    po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
Listens for FinalBlock messages and updates a database\n\
\nAllowed options");
    general.add_options()
        ("help,h", "produce help message")
        ("version,v", "print version string")
        ("config", po::value(&config_filenames), "Config file where options may be specified (can be specified more than once)")
        ;

    po::options_description behavior("Identity and Behavior Options");
    behavior.add_options()
        ("debug-mode", po::value<std::string>(), "Debug mode (on|toy|perf) for testing")
        ("mode", po::value<std::string>(), "Devv mode (T1|T2|scan)")
        ("shard-index", po::value<unsigned int>(), "Index of this shard")
        ("num-consensus-threads", po::value<unsigned int>(), "Number of consensus threads")
        ("num-validator-threads", po::value<unsigned int>(), "Number of validation threads")
        ("host-list,host", po::value<std::vector<std::string>>(),
         "Client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple nodes.")
        ("stop-file", po::value<std::string>(), "A file indicating that this process should stop.")
        ("update-host", po::value<std::string>(), "DNS host of database to update.")
        ("update-ip", po::value<std::string>(), "IP Address of database to update.")
        ("update-db", po::value<std::string>(), "Database name to update.")
        ("update-user", po::value<std::string>(), "Database username to use for updates.")
        ("update-pass", po::value<std::string>(), "Database password to use for updates.")
        ("update-port", po::value<unsigned int>(), "Database port to use for updates.")
        ;

    po::options_description all_options;
    all_options.add(general);
    all_options.add(behavior);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                  options(all_options).
                  run(),
              vm);

    if (vm.count("help")) {
      LOG_INFO << all_options;
      return nullptr;
    }

    if(vm.count("config") > 0)
    {
      config_filenames = vm["config"].as<std::vector<std::string> >();

      for(size_t i = 0; i < config_filenames.size(); ++i)
      {
        std::ifstream ifs(config_filenames[i].c_str());
        if(ifs.fail())
        {
          LOG_ERROR << "Error opening config file: " << config_filenames[i];
          return nullptr;
        }
        po::store(po::parse_config_file(ifs, all_options), vm);
      }
    }

    po::store(po::parse_command_line(argc, argv, all_options), vm);
    po::notify(vm);

    if (vm.count("mode")) {
      std::string mode = vm["mode"].as<std::string>();
      if (mode == "SCAN") {
        options->mode = scan;
      } else if (mode == "T1") {
        options->mode = T1;
      } else if (mode == "T2") {
        options->mode = T2;
      } else {
        LOG_WARNING << "unknown mode: " << mode;
      }
      LOG_INFO << "mode: " << options->mode;
    } else {
      LOG_INFO << "mode was not set.";
    }

    if (vm.count("debug-mode")) {
      std::string debug_mode = vm["debug-mode"].as<std::string>();
      if (debug_mode == "on") {
        options->debug_mode = on;
      } else if (debug_mode == "toy") {
        options->debug_mode = toy;
      } else {
        options->debug_mode = off;
      }
      LOG_INFO << "debug_mode: " << options->debug_mode;
    } else {
      LOG_INFO << "debug_mode was not set.";
    }

    if (vm.count("shard-index")) {
      options->shard_index = vm["shard-index"].as<unsigned int>();
      LOG_INFO << "Shard index: " << options->shard_index;
    } else {
      LOG_INFO << "Shard index was not set.";
    }

    if (vm.count("host-list")) {
      options->host_vector = vm["host-list"].as<std::vector<std::string>>();
      LOG_INFO << "Node URIs:";
      for (auto i : options->host_vector) {
        LOG_INFO << "  " << i;
      }
    }

    if (vm.count("stop-file")) {
      options->stop_file = vm["stop-file"].as<std::string>();
      LOG_INFO << "Stop file: " << options->stop_file;
    } else {
      LOG_INFO << "Stop file was not set. Use a signal to stop the node.";
    }

    if (vm.count("update-host")) {
      options->db_host = vm["update-host"].as<std::string>();
      LOG_INFO << "Database host: " << options->db_host;
    } else {
      LOG_INFO << "Database host was not set.";
    }

    if (vm.count("update-ip")) {
      options->db_ip = vm["update-ip"].as<std::string>();
      LOG_INFO << "Database IP: " << options->db_ip;
    } else {
      LOG_INFO << "Database IP was not set.";
    }

    if (vm.count("update-db")) {
      options->db_name = vm["update-db"].as<std::string>();
      LOG_INFO << "Database name: " << options->db_name;
    } else {
      LOG_INFO << "Database name was not set.";
    }

    if (vm.count("update-user")) {
      options->db_user = vm["update-user"].as<std::string>();
      LOG_INFO << "Database user: " << options->db_user;
    } else {
      LOG_INFO << "Database user was not set.";
    }

    if (vm.count("update-pass")) {
      options->db_pass = vm["update-pass"].as<std::string>();
      LOG_INFO << "Database pass: " << options->db_pass;
    } else {
      LOG_INFO << "Database pass was not set.";
    }

    if (vm.count("update-port")) {
      options->db_port = vm["update-port"].as<unsigned int>();
      LOG_INFO << "Database port: " << options->db_port;
    } else {
      LOG_INFO << "Database port was not set, default to 5432.";
      options->db_port = 5432;
    }

  }
  catch(std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
