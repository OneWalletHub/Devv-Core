/*
 * devv-announcer.cpp listens for Devv Protobuf envelopes.
 * These envelopes include transaction files that devv-announcer
 * annouonces to nodes provided by the host-list arguments.
 * The envelopes also contain oracle proposals that are used
 * to invoke oracle logic.
 *
 * @copywrite  2018 Devvio Inc
 */

#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "common/logger.h"
#include "common/devv_context.h"
#include "io/message_service.h"
#include "modules/BlockchainModule.h"
#include "primitives/block_tools.h"
#include "pbuf/devv_pbuf.h"

using namespace Devv;

namespace fs = boost::filesystem;

#define DEBUG_TRANSACTION_INDEX (processed + 11000000)

void ParallelPush(std::mutex& m, std::vector<std::vector<byte>>& array
    , const std::vector<byte>& elt) {
  std::lock_guard<std::mutex> guard(m);
  LOG_DEBUG << "ParallelPush: Pushing size " << elt.size();
  array.push_back(elt);
}

/**
 * Holds command-line options
 */
struct announcer_options {
  std::string bind_endpoint;
  std::string protobuf_endpoint;
  eAppMode mode;
  unsigned int node_index;
  unsigned int shard_index;
  std::string inn_keys;
  std::string node_keys;
  std::string key_pass;
  std::string stop_file;
  eDebugMode debug_mode;
  unsigned int batch_size;
  unsigned int start_delay;
  bool distinct_ops;
  std::string working_dir;
};

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct announcer_options> ParseAnnouncerOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  try {
    auto options = ParseAnnouncerOptions(argc, argv);

    if (!options) {
      LOG_ERROR << "ParseAnnouncerOptions error";
      exit(-1);
    }

    zmq::context_t context(1);

    DevvContext this_context(options->node_index,
                                options->shard_index,
                                options->mode,
                                options->inn_keys,
                                options->node_keys,
                                options->key_pass);

    KeyRing keys(this_context);

    MTR_SCOPE_FUNC();
    std::vector<std::vector<byte>> transactions;

    auto server = io::CreateTransactionServer(options->bind_endpoint, context);
    server->startServer();

    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind (options->protobuf_endpoint);

    std::string shard_name = "shard-"+std::to_string(options->shard_index);
    Blockchain chain(shard_name);
    chain.Fill(options->working_dir, keys, options->mode);

    //listen for FinalBlocks
    auto peer_listener = io::CreateTransactionClient(options->bind_endpoint, context);
    peer_listener->attachCallback([&](DevvMessageUniquePtr p) {
      if (p->message_type == eMessageType::FINAL_BLOCK) {
        try {
          ChainState prior = chain.getHighestChainState();
          InputBuffer buffer(p->data);
          auto top_block = std::make_shared<FinalBlock>(buffer, prior
                                                       , keys, options->mode);
          chain.push_back(top_block);
	    } catch (const std::exception& e) {
          std::exception_ptr p = std::current_exception();
          std::string err("");
          err += (p ? p.__cxa_exception_type()->name() : "null");
          LOG_WARNING << "Error: " + err << std::endl;
        }
      }
    });

    if (options->start_delay > 0) sleep(options->start_delay);

    bool keep_running = true;
    unsigned int processed_total = 0;
    while (keep_running) {
      zmq::message_t transaction_message;
      //  Wait for next request from client

      LOG_INFO << "Waiting for envelope";
      auto res = socket.recv(&transaction_message);
      if (!res) {
        LOG_ERROR << "socket.recv != true - exiting";
        keep_running = false;
      }
      LOG_INFO << "Received envelope";

      std::string tx_string = std::string(static_cast<char*>(transaction_message.data()),
          transaction_message.size());

      std::string response;
      std::vector<TransactionPtr> ptrs;
      try {
        ptrs = DeserializeEnvelopeProtobufString(tx_string, keys);
      } catch (std::runtime_error& e) {
        response = "Deserialization error: " + std::string(e.what());
        LOG_ERROR << response;
        zmq::message_t reply(response.size());
        memcpy(reply.data(), response.data(), response.size());
        socket.send(reply);
        continue;
      }

      unsigned int processed = 0;
      std::vector<DevvMessageUniquePtr> messages;
      for (auto const& t2tx : ptrs) {
        LOG_DEBUG << "Announce signature: "+t2tx->getSignature().getJSON();
        auto announce_msg = std::make_unique<DevvMessage>(
            this_context.get_shard_uri(),
            TRANSACTION_ANNOUNCEMENT,
            t2tx->getCanonical(),
            DEBUG_TRANSACTION_INDEX);

        LOG_DEBUG << "Going to queue";
        server->queueMessage(std::move(announce_msg));
        LOG_DEBUG << "Sent transaction batch #" << processed;
        ++processed;

        if (fs::exists(options->stop_file)) {
          LOG_INFO << "Shutdown file exists. Stopping pb_announcer...";
          keep_running = false;
        }
      }
      processed_total += processed;
      LOG_DEBUG << "Finished publishing transactions (processed/total) (" +
            std::to_string(processed) + "/" +
            std::to_string(processed_total) + ")";

      response = "Successfully published " + std::to_string(processed) + " transactions.";

      zmq::message_t reply(response.size());
      memcpy(reply.data(), response.data(), response.size());
      socket.send(reply);
    }

    LOG_INFO << "Finished running";
    sleep(1);
    server->stopServer();
    LOG_WARNING << "All done.";
    return (true);
  } catch (const std::exception& e) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: " + err << std::endl;
    std::cerr << err << std::endl;
    return (false);
  }
}

std::unique_ptr<struct announcer_options> ParseAnnouncerOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<announcer_options> options(new announcer_options());
  std::vector<std::string> config_filenames;

  try {
    po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Reads Devv transaction files from a directory and \n\
annouonces them to nodes provided by the host-list arguments.\n\
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
        ("node-index", po::value<unsigned int>(), "Index of this node")
        ("shard-index", po::value<unsigned int>(), "Index of this shard")
        ("bind-endpoint", po::value<std::string>(), "Endpoint for validator server (i.e. tcp://*:5556)")
        ("protobuf-endpoint", po::value<std::string>(), "Endpoint for protobuf server (i.e. tcp://*:5557)")
        ("inn-keys", po::value<std::string>(), "Path to INN key file")
        ("node-keys", po::value<std::string>(), "Path to Node key file")
        ("key-pass", po::value<std::string>(), "Password for private keys")
        ("stop-file", po::value<std::string>(), "When this file exists it indicates that this announcer should stop.")
        ("start-delay", po::value<unsigned int>(), "Sleep time before starting (millis)")
        ("separate-ops", po::value<bool>(), "Separate transactions with different operations into distinct batches?")
        ("working-dir", po::value<std::string>(), "Directory containing blockchain(s)")
        ;

    po::options_description all_options;
    all_options.add(general);
    all_options.add(behavior);

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv).
                  options(all_options).allow_unregistered().
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
        po::store(po::parse_config_file(ifs, all_options, true), vm);
      }
    }

   po::store(po::command_line_parser(argc, argv).
                  options(all_options).allow_unregistered().
                  run(),
              vm);
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

    if (vm.count("node-index")) {
      options->node_index = vm["node-index"].as<unsigned int>();
      LOG_INFO << "Node index: " << options->node_index;
    } else {
      LOG_INFO << "Node index was not set.";
    }

    if (vm.count("shard-index")) {
      options->shard_index = vm["shard-index"].as<unsigned int>();
      LOG_INFO << "Shard index: " << options->shard_index;
    } else {
      LOG_INFO << "Shard index was not set.";
    }

    if (vm.count("bind-endpoint")) {
      options->bind_endpoint = vm["bind-endpoint"].as<std::string>();
      LOG_INFO << "Bind URI: " << options->bind_endpoint;
    } else {
      LOG_INFO << "Bind URI was not set";
    }

    if (vm.count("protobuf-endpoint")) {
      options->protobuf_endpoint = vm["protobuf-endpoint"].as<std::string>();
      LOG_INFO << "Protobuf Endpoint: " << options->protobuf_endpoint;
    } else {
      LOG_INFO << "Protobuf Endpoint was not set";
    }

    if (vm.count("inn-keys")) {
      options->inn_keys = vm["inn-keys"].as<std::string>();
      LOG_INFO << "INN keys file: " << options->inn_keys;
    } else {
      LOG_INFO << "INN keys file was not set.";
    }

    if (vm.count("node-keys")) {
      options->node_keys = vm["node-keys"].as<std::string>();
      LOG_INFO << "Node keys file: " << options->node_keys;
    } else {
      LOG_INFO << "Node keys file was not set.";
    }

    if (vm.count("key-pass")) {
      options->key_pass = vm["key-pass"].as<std::string>();
      LOG_INFO << "Key pass: " << options->key_pass;
    } else {
      LOG_INFO << "Key pass was not set.";
    }

    if (vm.count("stop-file")) {
      options->stop_file = vm["stop-file"].as<std::string>();
      LOG_INFO << "Stop file: " << options->stop_file;
    } else {
      LOG_INFO << "Stop file was not set. Use a signal to stop the node.";
    }

    if (vm.count("start-delay")) {
      options->start_delay = vm["start-delay"].as<unsigned int>();
      LOG_INFO << "Start delay: " << options->start_delay;
    } else {
      LOG_INFO << "Start delay was not set (default to no delay).";
      options->start_delay = 0;
    }

    if (vm.count("separate-ops")) {
      options->distinct_ops = vm["separate-ops"].as<bool>();
      LOG_INFO << "Separate admin ops: " << options->distinct_ops;
    } else {
      LOG_INFO << "Separate ops was not set (default to no separation).";
      options->distinct_ops = false;
    }

    if (vm.count("working-dir")) {
      options->working_dir = vm["working-dir"].as<std::string>();
      LOG_INFO << "Working dir: " << options->working_dir;
    } else {
      LOG_INFO << "Working dir was not set.";
    }

  }
  catch(std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
