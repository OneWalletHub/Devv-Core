/*
 * reset-db.cpp resets the chain state of a shard
 * by truncating tables in a devv postgres-compliant database.
 * @copywrite  2018 Devvio Inc
 */

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <memory>

#include <boost/program_options.hpp>
#include <pqxx/pqxx>

#include "common/logger.h"
#include "common/devv_context.h"

using namespace Devv;

typedef std::chrono::milliseconds millisecs;

/**
 * Holds command-line options
 */
struct reset_options {
  std::string db_user;
  std::string db_pass;
  std::string db_host;
  std::string db_ip;
  std::string db_name;
  unsigned int db_port = 5432;
};

static const std::string kNIL_UUID = "00000000-0000-0000-0000-000000000000";
static const std::string kNIL_UUID_PSQL = "'00000000-0000-0000-0000-000000000000'::uuid";
static const std::string kRESET_CHAINSTATE = "reset_chainstate";
static const std::string kRESET_CHAINSTATE_PSQL = "SELECT reset_state()";

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct reset_options> ParseResetOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  try {
    auto options = ParseResetOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    MTR_SCOPE_FUNC();

    bool db_connected = false;
    std::unique_ptr<pqxx::connection> db_link = nullptr;
    if ((options->db_host.size() > 0) && (options->db_user.size() > 0)) {
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
        LOG_FATAL << "Database connection failed: " + db_params;
        throw;
      }
      //comments in pqxx say to resist the urge to immediately call
      //db_link->is_open(), if there was no exception above the
      //connection should be established
      LOG_INFO << "Successfully connected to database.";
      db_connected = true;
      pqxx::work stmt(*db_link);
      stmt.exec(kRESET_CHAINSTATE_PSQL);
      stmt.commit();
      LOG_INFO << "Chainstate reset in database.  Ready for new chain.";
    } else {
      LOG_FATAL << "Database host and user not set!";
    }

    if (db_connected) db_link->disconnect();
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


std::unique_ptr<struct reset_options> ParseResetOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<reset_options> options(new reset_options());
  std::vector<std::string> config_filenames;

  try {
    po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
Updates a database to reset chainstate\n\
\nAllowed options");
    general.add_options()
        ("help,h", "produce help message")
        ("version,v", "print version string")
        ("config", po::value(&config_filenames), "Config file where options may be specified (can be specified more than once)")
        ;

    po::options_description behavior("Identity and Behavior Options");
    behavior.add_options()
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
      LOG_INFO << "Database user: " << options->db_host;
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
      LOG_INFO << "Database pass: " << options->db_port;
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
