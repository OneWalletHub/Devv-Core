/*
 * devv-psql.cpp listens for FinalBlock messages and updates an INN database
 * based on the transactions in those blocks.
 * This process is designed to integrate with a postgres database.
 *
 * @copywrite  2018 Devvio Inc
 */

#include <atomic>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>
#include <boost/thread/thread.hpp>
#include <boost/asio.hpp>

#include <pqxx/pqxx>
#include <pqxx/nontransaction>

#include "common/logger.h"
#include "common/devv_context.h"
#include "io/message_service.h"
#include "io/psql_api.h"

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

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::shared_ptr<struct psql_options> ParsePsqlOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  std::shared_ptr<psql_options> options = nullptr;

  options = ParsePsqlOptions(argc, argv);

  if (!options) {
    exit(-1);
  }

  zmq::context_t zmq_context(1);

  MTR_SCOPE_FUNC();

  std::string shard_name = "Shard-"+std::to_string(options->shard_index);

  PSQLInterface psql_interface(options->db_host,
                               options->db_ip,
                               options->db_port,
                               options->db_name,
                               options->db_user,
                               options->db_pass,
                               shard_name
  );

  // @todo(nick@devv.io): read pre-existing chain
  ThreadedDBServer db_server;

  db_server.attachCallback([&](ConstFinalBlockSharedPtr block, size_t height) {
      psql_interface.handleNextBlock(block, height);
    }
  );

  BlockchainPtr chain = std::make_shared<Blockchain>(shard_name);
  KeyRing keys;

  auto peer_listener = io::CreateTransactionClient(options->host_vector, zmq_context);
  peer_listener->attachCallback([&](DevvMessageUniquePtr p) {
    if (p->message_type == eMessageType::FINAL_BLOCK) {
      try {
        ChainState prior = chain->getHighestChainState();
        InputBuffer buffer(p->data);
        auto top_block = std::make_shared<FinalBlock>(buffer, prior, keys, options->mode);
        db_server.push_queue(top_block);
        chain->push_back(top_block);
      } catch (const std::exception& e) {
        std::exception_ptr p = std::current_exception();
        std::string err("");
        err += (p ? p.__cxa_exception_type()->name() : "null");
        LOG_WARNING << "Error: " + err << std::endl;
      }
    }
  });
  peer_listener->listenTo(get_shard_uri(options->shard_index));
  peer_listener->startClient();
  LOG_INFO << "devv-psql is listening to shard: "+get_shard_uri(options->shard_index);

  auto ms = kMAIN_WAIT_INTERVAL;
  int dolog = 0;
  while (true) {
    if (dolog++ % 50 == 0)
    {
      LOG_DEBUG << "devv-psql sleeping for " << ms;
    }
    std::this_thread::sleep_for(millisecs(ms));
    /* Should we shutdown? */
    if (fs::exists(options->stop_file)) {
      LOG_INFO << "Shutdown file exists. Stopping devv-psql...";
      break;
    }
  }
  peer_listener->stopClient();
  return (true);
}

std::shared_ptr<struct psql_options> ParsePsqlOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::shared_ptr<psql_options> options(new psql_options());
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
