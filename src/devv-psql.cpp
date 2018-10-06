
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
  std::string db_name;
  unsigned int db_port = 5432;
};

static const std::string kNIL_UUID = "00000000-0000-0000-0000-000000000000";
static const std::string kNIL_UUID_PSQL = "'00000000-0000-0000-0000-000000000000'::uuid";
static const std::string kGET_UUID = "get_uuid";
static const std::string kGET_UUID_STATEMENT = "select devv_uuid()";
static const std::string kTX_INSERT = "tx_insert";
static const std::string kTX_INSERT_STATEMENT = "INSERT INTO tx (tx_id, shard_id, block_height, wallet_id, coin_id, amount) (select cast($1 as uuid), $2, $3, wallet_id, $4, $5 from wallet where wallet_addr = $6)";
static const std::string kRX_INSERT = "rx_insert";
static const std::string kRX_INSERT_STATEMENT = "INSERT INTO rx (rx_id, shard_id, block_height, wallet_id, coin_id, amount, tx_id) (select devv_uuid(), $1, $2, wallet_id, $3, $4, $5 from wallet where wallet_addr = $6)";
static const std::string kBALANCE_SELECT = "balance_select";
static const std::string kBALANCE_SELECT_STATEMENT = "select wc.balance from wallet_coin wc, wallet w where w.wallet_id = wc.wallet_id and w.wallet_addr = $1 and wc.coin_id = $2";
static const std::string kWALLET_INSERT = "wallet_insert";
static const std::string kWALLET_INSERT_STATEMENT = "INSERT INTO wallet (wallet_id, wallet_addr, account_id, wallet_name, shard_id) (select devv_uuid(), $1, '"+kNIL_UUID+"', 'None', $2)";
static const std::string kBALANCE_INSERT = "balance_insert";
static const std::string kBALANCE_INSERT_STATEMENT = "INSERT INTO wallet_coin (wallet_coin_id, wallet_id, coin_id, account_id, balance, block_height) (select devv_uuid(), wallet_id, $1, $2, $3, $4 from wallet where wallet_addr = $5)";
static const std::string kBALANCE_UPDATE = "balance_update";
static const std::string kBALANCE_UPDATE_STATEMENT = "UPDATE wallet_coin set balance = $1, block_height = $2 where wallet_id in (select wallet_id from wallet where wallet_addr = $3) and coin_id = $4";
static const std::string kSHARD_SELECT = "shard_select";
static const std::string kSHARD_SELECT_STATEMENT = "select shard_id from shard where shard_name = $1";

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct psql_options> ParsePsqlOptions(int argc, char** argv);

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
    if (!options->db_host.empty() && !options->db_user.empty()) {
      const std::string db_params("dbname = "+options->db_name +
          " user = "+options->db_user+
          " password = "+options->db_pass +
          " hostaddr = "+options->db_host +
          " port = "+std::to_string(options->db_port));
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
      db_link->prepare(kGET_UUID, kGET_UUID_STATEMENT);
      db_link->prepare(kTX_INSERT, kTX_INSERT_STATEMENT);
      db_link->prepare(kRX_INSERT, kRX_INSERT_STATEMENT);
      db_link->prepare(kWALLET_INSERT, kWALLET_INSERT_STATEMENT);
      db_link->prepare(kBALANCE_SELECT, kBALANCE_SELECT_STATEMENT);
      db_link->prepare(kBALANCE_INSERT, kBALANCE_INSERT_STATEMENT);
      db_link->prepare(kBALANCE_UPDATE, kBALANCE_UPDATE_STATEMENT);
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
          std::vector<TransactionPtr> txs = one_block.CopyTransactions();

          for (TransactionPtr& one_tx : txs) {

            pqxx::work stmt(*db_link);
            std::vector<TransferPtr> xfers = one_tx->getTransfers();
            std::string sig_str(one_tx->getSignature().getCanonical().begin()
                              , one_tx->getSignature().getCanonical().end());
            std::string sender_str;
            uint64_t coin_id = 0;
            int64_t send_amount = 0;

            for (TransferPtr& one_xfer : xfers) {

              if (one_xfer->getAmount() < 0) {
                std::string temp(one_xfer->getAddress().getCanonical().begin()
                               , one_xfer->getAddress().getCanonical().end());
                sender_str = temp;
                coin_id = one_xfer->getCoin();
                send_amount = one_xfer->getAmount();
                break;
              }
            } //end sender search loop

            //update sender balance
            pqxx::result balance_result = stmt.prepared(kBALANCE_SELECT)(sender_str)(coin_id).exec();
            if (balance_result.empty()) {
              stmt.prepared(kBALANCE_INSERT)(coin_id)(kNIL_UUID_PSQL)(send_amount)(chain_height)(sender_str).exec();
            } else {
              int64_t new_balance = balance_result[0][0].as<int64_t>()-send_amount;
              stmt.prepared(kBALANCE_UPDATE)(new_balance)(chain_height)(sender_str)(coin_id).exec();
            }
            pqxx::result uuid_result = stmt.prepared(kGET_UUID).exec();
            if (!uuid_result.empty()) {
              std::string tx_uuid = uuid_result[0][0].as<std::string>();
              stmt.prepared(kTX_INSERT)(tx_uuid)(options->shard_index)(chain_height)(coin_id)(send_amount)(sender_str).exec();
              for (TransferPtr& one_xfer : xfers) {
                int64_t amount = one_xfer->getAmount();
                uint64_t delay = one_xfer->getDelay();
                //not a receiver
                if (amount < 0) continue;
                //there's a delay
                //@todo(nick@devv.io) handle delayed settlements, tx in pending_tx
                if (delay > 0) continue;
                //update receiver balance
                std::string receiver_str(one_xfer->getAddress().getCanonical().begin()
                  , one_xfer->getAddress().getCanonical().end());
                pqxx::work rx_stmt(*db_link);
                pqxx::result rx_balance = rx_stmt.prepared(kBALANCE_SELECT)(receiver_str)(coin_id).exec();
                if (rx_balance.empty()) {
                  stmt.prepared(kBALANCE_INSERT)(coin_id)(kNIL_UUID_PSQL)(amount)(receiver_str).exec();
                } else {
                  int64_t new_balance = balance_result[0][0].as<int64_t>()+amount;
                  stmt.prepared(kBALANCE_UPDATE)(new_balance)(chain_height)(receiver_str)(coin_id).exec();
                }
                stmt.prepared(kRX_INSERT)(options->shard_index)(chain_height)(coin_id)(send_amount)(tx_uuid)(receiver_str).exec();
              } //end transfer loop
            } else {
              LOG_WARNING << "Failed to generate a UUID!";
            }
            stmt.commit();
          } //end transaction loop
        } else { //endif db connected?
          throw std::runtime_error("Database is not connected!");
        }
        chain_height++;
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
    LOG_FATAL << "Error: " + err << std::endl;
    std::cerr << err << std::endl;
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
        ("update-host", po::value<std::string>(), "Host of database to update.")
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
