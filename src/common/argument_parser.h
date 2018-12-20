/*
 * argument-parser.h parses arguments for a Devv core validator.
 *
 * @copywrite  2018 Devvio Inc
 */

#pragma once

#ifndef COMMON_ARGUMENT_PARSER_H_
#define COMMON_ARGUMENT_PARSER_H_

#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <fstream>
#include <exception>
#include <algorithm>

#include <boost/program_options.hpp>

#include "devv_context.h"
#include "common/logger.h"

namespace Devv {

template <typename T>
void RemoveDuplicates(std::vector<T>& vec)
{
  std::sort(vec.begin(), vec.end());
  vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
}

struct devv_options {
  std::string bind_endpoint;
  std::vector<std::string> host_vector{};
  eAppMode mode = T1;
  unsigned int node_index = 0;
  unsigned int shard_index = 0;
  unsigned int num_consensus_threads = 1;
  unsigned int num_validator_threads = 1;
  unsigned int batch_size;
  unsigned int max_wait;
  unsigned int syslog_port;
  std::string working_dir;
  std::string trace_file;
  std::string inn_keys;
  std::string node_keys;
  std::string key_pass;
  std::string stop_file;
  std::string syslog_host;
  eDebugMode debug_mode = off;
};

inline std::unique_ptr<struct devv_options> ParseDevvOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<devv_options> options(new devv_options());
  std::vector<std::string> config_filenames;

  try {
    po::options_description general("General Options");
    general.add_options()
        ("help,h", "produce help message")
        ("version,v", "print version string")
        ("config", po::value(&config_filenames), "Config file where options may be specified (can be specified more than once)")
        ;

    po::options_description behavior("Identity and Behavior Options");
    behavior.add_options()
        ("mode", po::value<std::string>(), "Devv mode (T1|T2)")
        ("node-index", po::value<unsigned int>(), "Index of this node")
        ("shard-index", po::value<unsigned int>(), "Index of this shard")
        ("host-list,H", po::value<std::vector<std::string>>()->composing(),
         "Client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple nodes.")
        ("bind-endpoint", po::value<std::string>(), "Endpoint for server (i.e. tcp://*:5556)")
        ("inn-keys", po::value<std::string>(), "Path to INN key file")
        ("node-keys", po::value<std::string>(), "Path to Node key file")
        ("key-pass", po::value<std::string>(), "Password for private keys")
        ;

    po::options_description debugging("Debugging and Performance Options");
    debugging.add_options()
        ("debug-mode", po::value<std::string>(), "Debug mode (on|toy|perf) for testing")
        ("trace-output", po::value<std::string>(), "Output path to JSON trace file (Chrome)")
        ("num-consensus-threads", po::value<unsigned int>(), "Number of consensus threads")
        ("num-validator-threads", po::value<unsigned int>(), "Number of validation threads")
        ("working-dir", po::value<std::string>(), "Directory where inputs are read and outputs are written")
        ("stop-file", po::value<std::string>(), "A file in working-dir indicating that this node should stop.")
        ("tx-batch-size", po::value<unsigned int>(), "Target size of transaction batches")
        ("max-wait", po::value<unsigned int>(), "Maximum number of millis to wait for transactions")
        ("syslog-host", po::value<std::string>(), "The syslog host name")
        ("syslog-port", po::value<unsigned int>(), "The syslog port number")
        ;

    po::options_description all_options;
    all_options.add(general);
    all_options.add(behavior);
    all_options.add(debugging);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                  options(all_options).
                  run(),
              vm);

    if (vm.count("help")) {
      std::cout << all_options;
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
          std::cerr << "Error opening config file: " << config_filenames[i] << std::endl;
          return nullptr;
        }
        po::store(po::parse_config_file(ifs, all_options), vm);
      }
    }

    po::store(po::parse_command_line(argc, argv, all_options), vm);
    po::notify(vm);

    if (vm.count("mode")) {
      std::string mode = vm["mode"].as<std::string>();
      if (mode == "T1") {
        options->mode = T1;
      } else if (mode == "T2") {
        options->mode = T2;
      } else {
        std::cerr << "unknown mode: " << mode << std::endl;
      }
      std::cout << "mode: " << options->mode << std::endl;
    } else {
      std::cerr << "mode was not set." << std::endl;
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
      std::cout << "debug_mode: " << options->debug_mode << std::endl;
    } else {
      std::cout << "debug_mode was not set." << std::endl;
    }

    if (vm.count("node-index")) {
      options->node_index = vm["node-index"].as<unsigned int>();
      std::cout << "Node index: " << options->node_index << std::endl;
    } else {
      std::cout << "Node index was not set." << std::endl;
    }

    if (vm.count("shard-index")) {
      options->shard_index = vm["shard-index"].as<unsigned int>();
      std::cout << "Shard index: " << options->shard_index << std::endl;
    } else {
      std::cout << "Shard index was not set." << std::endl;
    }

    if (vm.count("num-consensus-threads")) {
      options->num_consensus_threads = vm["num-consensus-threads"].as<unsigned int>();
      std::cout << "Num consensus threads: " << options->num_consensus_threads << std::endl;
    } else {
      std::cout << "Num consensus threads was not set, defaulting to 10" << std::endl;
      options->num_consensus_threads = 10;
    }

    if (vm.count("num-validator-threads")) {
      options->num_validator_threads = vm["num-validator-threads"].as<unsigned int>();
      std::cout << "Num validator threads: " << options->num_validator_threads << std::endl;
    } else {
      std::cout << "Num validator threads was not set, defaulting to 10" << std::endl;
      options->num_validator_threads = 10;
    }

    if (vm.count("tx-batch-size")) {
      options->batch_size = vm["tx-batch-size"].as<unsigned int>();
      std::cout << "Batch size: " << options->batch_size << std::endl;
    } else {
      std::cout << "Batch size was not set (default to 10000)." << std::endl;
      options->batch_size = 10000;
    }

    if (vm.count("max-wait")) {
      options->max_wait = vm["max-wait"].as<unsigned int>();
      std::cout << "Max Wait: " << options->max_wait << std::endl;
    } else {
      std::cout << "Max Wait was not set (default to no wait)." << std::endl;
      options->max_wait = 0;
    }

    if (vm.count("bind-endpoint")) {
      options->bind_endpoint = vm["bind-endpoint"].as<std::string>();
      std::cout << "Bind URI: " << options->bind_endpoint << std::endl;
    } else {
      std::cout << "Bind URI was not set" << std::endl;
    }

    if (vm.count("host-list")) {
      options->host_vector = vm["host-list"].as<std::vector<std::string>>();
      RemoveDuplicates(options->host_vector);
      std::cout << "Node URIs:" << std::endl;
      for (auto i : options->host_vector) {
        std::cout << "  " << i << std::endl;
      }
    }

    if (vm.count("working-dir")) {
      options->working_dir = vm["working-dir"].as<std::string>();
      std::cout << "Working dir: " << options->working_dir << std::endl;
    } else {
      std::cout << "Working dir was not set." << std::endl;
    }

    if (vm.count("trace-output")) {
      options->trace_file = vm["trace-output"].as<std::string>();
      std::cout << "Trace output file: " << options->trace_file << std::endl;
    } else {
      std::cout << "Trace file was not set." << std::endl;
    }

    if (vm.count("inn-keys")) {
      options->inn_keys = vm["inn-keys"].as<std::string>();
      std::cout << "INN keys file: " << options->inn_keys << std::endl;
    } else {
      std::cout << "INN keys file was not set." << std::endl;
    }

    if (vm.count("node-keys")) {
      options->node_keys = vm["node-keys"].as<std::string>();
      std::cout << "Node keys file: " << options->node_keys << std::endl;
    } else {
      std::cout << "Node keys file was not set." << std::endl;
    }

    if (vm.count("key-pass")) {
      options->key_pass = vm["key-pass"].as<std::string>();
      std::cout << "Key pass: " << options->key_pass << std::endl;
    } else {
      std::cout << "Key pass was not set." << std::endl;
    }

    if (vm.count("stop-file")) {
      options->stop_file = vm["stop-file"].as<std::string>();
      std::cout << "Stop file: " << options->stop_file << std::endl;
    } else {
      std::cout << "Stop file was not set. Use a signal to stop the node." << std::endl;
    }

    if (vm.count("syslog-host")) {
      options->syslog_host = vm["syslog-host"].as<std::string>();
      std::cout << "Syslog host: " << options->syslog_host << std::endl;
    } else {
      std::cout << "Syslog host was not set - disabling syslog logging." << std::endl;
    }

    if (vm.count("syslog-port")) {
      options->syslog_port = vm["syslog-port"].as<unsigned int>();
      std::cout << "Syslog port: " << options->syslog_port << std::endl;
    } else {
      options->syslog_port = kDEFAULT_SYSLOG_PORT;
      std::cout << "Syslog port not set (default to " << options->syslog_port << ")." << std::endl;
    }

  }
  catch(std::exception& e) {
    std::cerr << "error: " << e.what() << std::endl;
    return nullptr;
  }

  return options;
}

} // namespace Devv

#endif /* COMMON_ARGUMENT_PARSER_H_ */
