#pragma once

#include <vector>
#include <string>
#include <memory>
#include <iostream>
#include <exception>

#include <boost/program_options.hpp>

#include "devcash_context.h"
#include "logger.h"

namespace Devcash {

struct devcash_options {
  std::string bind_endpoint;
  std::vector<std::string> host_vector{};
  eAppMode mode;
  int node_index;
  unsigned int num_consensus_threads;
  unsigned int num_validator_threads;
  std::string scan_file;
  std::string write_file;
};

std::unique_ptr<struct devcash_options> parse_options(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<devcash_options> options(new devcash_options());

  try {
    po::options_description desc("\n\
The t1_example program can be used to demonstrate connectivity of\n\
T1 and T2 networks. By default, it will send random DevcashMessage\n\
every 5 seconds to any other t1_example programs that have connected\n\
to it will receive the message. If it receives a message, it will\n\
execute the appropriate T1 or T2 callback. t1_example can connect\n\
and listen to multiple other t1_example programs so almost any size\n\
network could be build and tested.\n\nAllowed options");
    desc.add_options()
      ("help", "produce help message")
      ("mode", po::value<std::string>()->required(), "Devcash mode (T1|T2|scan)")
      ("node-index", po::value<unsigned int>(), "Index of this node")
      ("num-consensus-threads", po::value<unsigned int>(), "Number of consensus threads")
      ("num-validator-threads", po::value<unsigned int>(), "Number of validation threads")
      ("host-list,host", po::value<std::vector<std::string>>(),
       "Client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple nodes.")
      ("bind-endpoint", po::value<std::string>(), "Endpoint for server (i.e. tcp://*:5556)")
      ("scan-file", po::value<std::string>(), "Initial transaction or blockchain input file")
      ("output", po::value<std::string>(), "Blockchain output path in binary JSON or CBOR")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      LOG_DEBUG << desc << "\n";
      return nullptr;
    }

    if (vm.count("mode")) {
      std::string mode = vm["mode"].as<std::string>();
      if (mode == "SCAN") {
        options->mode = scan;
      } else if (mode == "T1") {
        options->mode = T1;
      } else if (mode == "T2") {
        options->mode = T2;
      }
      LOG_DEBUG << "Mode: " << options->mode;
    } else {
      LOG_DEBUG << "Mode was not set.";
    }

    if (vm.count("node-index")) {
      options->node_index = vm["node-index"].as<unsigned int>();
      LOG_DEBUG << "Node index: " << options->node_index;
    } else {
      LOG_DEBUG << "Node index was not set.";
    }

    if (vm.count("num-consensus-threads")) {
      options->num_consensus_threads = vm["num-consensus-threads"].as<unsigned int>();
      LOG_DEBUG << "Num consensus threads: " << options->num_consensus_threads;
    } else {
      LOG_DEBUG << "Num consensus threads was not set, defaulting to 10";
      options->num_consensus_threads = 10;
    }

    if (vm.count("num-validator-threads")) {
      options->num_validator_threads = vm["num-validator-threads"].as<unsigned int>();
      LOG_DEBUG << "Num validator threads: " << options->num_validator_threads;
    } else {
      LOG_DEBUG << "Num validator threads was not set, defaulting to 10";
      options->num_validator_threads = 10;
    }

    if (vm.count("bind-endpoint")) {
      options->bind_endpoint = vm["bind-endpoint"].as<std::string>();
      LOG_DEBUG << "Bind URI: " << options->bind_endpoint;
    } else {
      LOG_DEBUG << "Bind URI was not set";
    }

    if (vm.count("host-list")) {
      options->host_vector = vm["host-list"].as<std::vector<std::string>>();
      LOG_DEBUG << "Node URIs:";
      for (auto i : options->host_vector) {
        LOG_DEBUG << "  " << i;
      }
    }

    if (vm.count("scan-file")) {
      options->scan_file = vm["scan-file"].as<std::string>();
      LOG_DEBUG << "Scan file: " << options->scan_file;
    } else {
      LOG_DEBUG << "Scan file was not set.";
    }

    if (vm.count("output")) {
      options->write_file = vm["output"].as<std::string>();
      LOG_DEBUG << "Output file: " << options->write_file;
    } else {
      LOG_DEBUG << "Output file was not set.";
    }

  }
  catch(std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
} // namespace Devcash