/*
 * devv-query.cpp listens for FinalBlock messages and saves them to a file
 * It responds to user requests to provide chain, shard, and block data.
 *
 * @copywrite  2018 Devvio Inc
 */

#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <string>
#include <sstream>
#include <thread>
#include <memory>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "common/logger.h"
#include "common/devv_context.h"
#include "common/devv_uri.h"
#include "consensus/blockchain.h"
#include "io/message_service.h"
#include "modules/BlockchainModule.h"
#include "pbuf/devv_pbuf.h"

using namespace Devv;

namespace fs = boost::filesystem;

#define DEBUG_TRANSACTION_INDEX (processed + 11000000)
typedef std::chrono::milliseconds millisecs;

/**
 * Holds command-line options
 */
struct repeater_options {
  std::vector<std::string> host_vector{};
  std::string protobuf_endpoint;
  eAppMode mode = eAppMode::T1;
  unsigned int node_index = 0;
  unsigned int shard_index = 0;
  unsigned int pad_amount = 0;
  std::string shard_name;
  std::string working_dir;
  std::string inn_keys;
  std::string node_keys;
  std::string key_pass;
  std::string stop_file;
  bool testnet = false;
  eDebugMode debug_mode = eDebugMode::off;
};

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct repeater_options> ParseRepeaterOptions(int argc, char** argv);

/**
 * Handle service request operations.
 * See the Repeater Interface page of the wiki for further information.
 * @param request - a smart pointer to the request
 * @param working_dir - the directory where blocks are stored
 * @param shard_name - the name of the shard this process tracks
 * @param shard_index - the index of the shard this process tracks
 * @param chain - the blockchain this shard tracks
 * @return ServiceResponsePtr - a smart pointer to the response
 */
ServiceResponsePtr HandleServiceRequest(const ServiceRequestPtr& request, const std::string& working_dir
    , const std::string& shard_name, unsigned int shard_index, const Blockchain& chain);

/**
 * Generate a bad syntax error response.
 * @param message - an error message to include with the response
 * @return RepeaterResponsePtr - a smart pointer to the response
 */
ServiceResponsePtr GenerateBadSyntaxResponse(std::string message) {
  ServiceResponse response;
  response.return_code = 1010;
  response.message = message;
  return std::make_unique<ServiceResponse>(response);
}

int main(int argc, char* argv[]) {
  init_log();

  try {
    auto options = ParseRepeaterOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    zmq::context_t zmq_context(1);

    DevvContext this_context(options->node_index, options->shard_index, options->mode, options->inn_keys,
                                options->node_keys, options->key_pass);
    KeyRing keys(this_context);

    LOG_DEBUG << "Write transactions to " << options->working_dir;
    MTR_SCOPE_FUNC();

    fs::path p(options->working_dir);

    if (!is_directory(p)) {
      LOG_ERROR << "Error opening dir: " << options->working_dir << " is not a directory";
      return false;
    }

    std::string shard_name = "Shard-"+std::to_string(options->shard_index);

    //@todo(nick@devv.io): read pre-existing chain
    Blockchain chain(options->shard_name);
    ChainState state;

    auto peer_listener = io::CreateTransactionClient(options->host_vector, zmq_context);
    peer_listener->attachCallback([&](DevvMessageUniquePtr p) {
      if (p->message_type == eMessageType::FINAL_BLOCK) {
        //write final chain to file
        std::string shard_dir(options->working_dir+"/"+this_context.get_shard_uri());
        fs::path dir_path(shard_dir);
        if (is_directory(dir_path)) {
          std::string block_height(std::to_string(chain.size()));
          std::string padded_height = block_height;
          if (block_height.length() < options->pad_amount) {
            padded_height = std::string(options->pad_amount - block_height.length(), '0')+block_height;
          }
          std::string out_file(shard_dir + "/" + padded_height + ".blk");
          std::ofstream block_file(out_file
            , std::ios::out | std::ios::binary);
          if (block_file.is_open()) {
            block_file.write((const char*) &p->data[0], p->data.size());
            block_file.close();
            LOG_DEBUG << "Wrote to " << out_file << "'.";
          } else {
            LOG_ERROR << "Failed to open output file '" << out_file << "'.";
          }
        } else {
          LOG_ERROR << "Error opening dir: " << shard_dir << " is not a directory";
        }
        try {
          InputBuffer buffer(p->data);
          FinalPtr top_block = std::make_shared<FinalBlock>(FinalBlock::Create(buffer, state));
          chain.push_back(top_block);
	    } catch (const std::exception& e) {
          std::exception_ptr p = std::current_exception();
          std::string err("");
          err += (p ? p.__cxa_exception_type()->name() : "null");
          LOG_WARNING << "Error: " + err << std::endl;
        }
      }
    });
    peer_listener->listenTo(this_context.get_shard_uri());
    peer_listener->startClient();
    LOG_INFO << "Devv-query is listening to shard: "+this_context.get_shard_uri();

    zmq::context_t context(1);
    zmq::socket_t socket (context, ZMQ_REP);
    socket.bind (options->protobuf_endpoint);

    unsigned int queries_processed = 0;
    while (true) {
      /* Should we shutdown? */
      if (fs::exists(options->stop_file)) {
        LOG_INFO << "Shutdown file exists. Stopping repeater...";
        break;
      }

      zmq::message_t request_message;
      //  Wait for next request from client

      LOG_INFO << "Waiting for ServiceRequest";
      auto res = socket.recv(&request_message);
      if (!res) {
        LOG_ERROR << "socket.recv != true - exiting";
        break;
      }
      LOG_INFO << "Received Message";
      std::string msg_string = std::string(static_cast<char*>(request_message.data()),
	            request_message.size());

      std::string response;
      ServiceRequestPtr request_ptr;
      try {
        request_ptr = DeserializeServiceRequest(msg_string);
      } catch (std::runtime_error& e) {
        ServiceResponsePtr response_ptr =
          GenerateBadSyntaxResponse("ServiceRequest Deserialization error: "
          + std::string(e.what()));
        std::stringstream response_ss;
        Devv::proto::ServiceResponse pbuf_response = SerializeServiceResponse(std::move(response_ptr));
        pbuf_response.SerializeToOstream(&response_ss);
        response = response_ss.str();
        zmq::message_t reply(response.size());
        memcpy(reply.data(), response.data(), response.size());
        socket.send(reply);
        continue;
      }

      ServiceResponsePtr response_ptr = HandleServiceRequest(std::move(request_ptr)
        , options->working_dir, options->shard_name
        , options->shard_index, chain);
      if (options->testnet) {
        response_ptr->args.insert(std::make_pair("testnet", "true"));
      }
      Devv::proto::ServiceResponse pbuf_response = SerializeServiceResponse(std::move(response_ptr));
      LOG_INFO << "Generated ServiceResponse, Serializing";
      std::stringstream response_ss;
      pbuf_response.SerializeToOstream(&response_ss);
      response = response_ss.str();
      zmq::message_t reply(response.size());
      memcpy(reply.data(), response.data(), response.size());
      socket.send(reply);
      queries_processed++;
      LOG_INFO << "ServiceResponse sent, process has handled: "
                +std::to_string(queries_processed)+" queries";
    }
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

std::unique_ptr<struct repeater_options> ParseRepeaterOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<repeater_options> options(new repeater_options());
  std::vector<std::string> config_filenames;

  try {
    po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
Listens for FinalBlock messages and saves them to a file\n\
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
        ("shard-name", po::value<std::string>(), "Name of this shard")
        ("num-consensus-threads", po::value<unsigned int>(), "Number of consensus threads")
        ("num-validator-threads", po::value<unsigned int>(), "Number of validation threads")
        ("host-list,host", po::value<std::vector<std::string>>(),
         "Client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple nodes.")
        ("protobuf-endpoint", po::value<std::string>(), "Endpoint for protobuf server (i.e. tcp://*:5557)")
        ("working-dir", po::value<std::string>(), "Directory where inputs are read and outputs are written")
        ("inn-keys", po::value<std::string>(), "Path to INN key file")
        ("node-keys", po::value<std::string>(), "Path to Node key file")
        ("key-pass", po::value<std::string>(), "Password for private keys")
        ("stop-file", po::value<std::string>(), "A file in working-dir indicating that this node should stop.")
        ("testnet", po::bool_switch()->default_value(false), "Set to true for the testnet.")
        ("pad-amount", po::value<unsigned int>(), "Zeros to leftpad block file names.")
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

    if (vm.count("shard-name")) {
      options->shard_name = vm["shard-name"].as<std::string>();
      LOG_INFO << "Shard name: " << options->shard_name;
    } else {
      LOG_INFO << "Shard name was not set.";
    }

    if (vm.count("host-list")) {
      options->host_vector = vm["host-list"].as<std::vector<std::string>>();
      LOG_INFO << "Node URIs:";
      for (auto i : options->host_vector) {
        LOG_INFO << "  " << i;
      }
    }

    if (vm.count("protobuf-endpoint")) {
      options->protobuf_endpoint = vm["protobuf-endpoint"].as<std::string>();
      LOG_INFO << "Protobuf Endpoint: " << options->protobuf_endpoint;
    } else {
      LOG_INFO << "Protobuf Endpoint was not set";
    }

    if (vm.count("working-dir")) {
      options->working_dir = vm["working-dir"].as<std::string>();
      LOG_INFO << "Working dir: " << options->working_dir;
    } else {
      LOG_INFO << "Working dir was not set.";
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

    if (vm.count("testnet")) {
      options->testnet = vm["testnet"].as<bool>();
      if (options->testnet) LOG_INFO << "TESTNET";
    }

    if (vm.count("pad-amount")) {
      options->pad_amount = vm["pad-amount"].as<unsigned int>();
      LOG_INFO << "Pad Amount: " << options->pad_amount;
    } else {
      options->pad_amount = 8;
      LOG_INFO << "Pad Amount was not set, default to 8.";
    }
  }
  catch(std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}

bool hasShard(std::string shard, const std::string& working_dir) {
  std::string shard_dir(working_dir+"/"+shard);
  fs::path dir_path(shard_dir);
  if (is_directory(dir_path)) return true;
  return false;
}

bool hasBlock(std::string shard, uint32_t block, const std::string& working_dir) {
  std::string block_path(working_dir+"/"+shard+"/"+std::to_string(block)+".blk");
  if (boost::filesystem::exists(block_path)) return true;
  return false;
}

uint32_t getHighestBlock(std::string shard, const std::string& working_dir) {
  std::string shard_dir(working_dir+"/"+shard);
  uint32_t highest = 0;
  for (auto i = boost::filesystem::directory_iterator(shard_dir);
      i != boost::filesystem::directory_iterator(); i++) {
    uint32_t some_block = std::stoul(i->path().stem().string(), nullptr, 10);
    if (some_block > highest) highest = some_block;
  }
  return highest;
}

std::vector<byte> ReadBlock(const std::string& shard, uint32_t block, const std::string& working_dir) {
  std::vector<byte> out;
  std::string block_path(working_dir+"/"+shard+"/"+std::to_string(block)+".blk");
  std::ifstream block_file(block_path, std::ios::in | std::ios::binary);
  block_file.unsetf(std::ios::skipws);

  std::streampos block_size;
  block_file.seekg(0, std::ios::end);
  block_size = block_file.tellg();
  block_file.seekg(0, std::ios::beg);
  out.reserve(block_size);

  out.insert(out.begin(),
             std::istream_iterator<byte>(block_file),
             std::istream_iterator<byte>());
  return out;
}

std::map<std::string, std::string> TraceTransactions(const std::string& shard
    , uint32_t start_block, uint32_t end_block
    , const std::vector<byte>& target, const std::string& working_dir) {
  std::map<std::string, std::string> txs;
  uint32_t highest = std::min(end_block, getHighestBlock(shard, working_dir));
  if (highest < start_block) return txs;

  for (uint32_t i=start_block; i<=highest; ++i) {
    std::vector<byte> block = ReadBlock(shard, i, working_dir);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    for (const auto& raw_tx : one_block.getRawTransactions()) {
      if(std::search(std::begin(raw_tx), std::end(raw_tx)
          , std::begin(target), std::end(target)) != std::end(raw_tx)) {
        InputBuffer t2_buffer(raw_tx);
        Tier2Transaction t2tx = Tier2Transaction::QuickCreate(t2_buffer);
        txs.insert(std::make_pair(ToHex(t2tx.getSignature().getCanonical()), ToHex(raw_tx)));
      }
    }
  }
  return txs;
}

std::string getServiceArgument(std::map<std::string, std::string>& args, std::string key) {
  auto it = args.find(key);
  if (it != args.end()) {
    return it->second;
  }
  return std::string();
}

ServiceResponsePtr HandleServiceRequest(const ServiceRequestPtr& request, const std::string& working_dir
    , const std::string& shard_name, unsigned int shard_index, const Blockchain& chain) {
  ServiceResponse response;
  try {
    response.request_timestamp = request->timestamp;
    response.endpoint = request->endpoint;
    if (request->args.empty()) {
      response.return_code = 1010;
      response.message = "Missing arguments.";
      return std::make_unique<ServiceResponse>(response);
    }
    std::string name = getServiceArgument(request->args, "shard-name");
    std::string shard_id = getServiceArgument(request->args, "shard-id");
    if (shard_name != name && shard_id != std::to_string(shard_index)) {
      response.return_code = 1050;
      response.message = "Shard '"+name+"' is not available.";
      return std::make_unique<ServiceResponse>(response);
    } else if (name.empty() && !hasShard(shard_id, working_dir)) {
      response.return_code = 1050;
      response.message = "Shard "+shard_id+" is not available.";
      return std::make_unique<ServiceResponse>(response);
    } else if (shard_id.empty()) {
      //TODO(nick@devv.io): devv-query should be able to track multiple shards,
      //but to do that it needs a map shard_name->shard_index/id
      shard_id = std::to_string(shard_index);
	}
	response.args.insert(std::make_pair("protocol-version", "0"));
	response.args.insert(std::make_pair("shard-name", shard_name));
    if (SearchString(request->endpoint, "/block-info", true)) {
      size_t height = std::stoi(getServiceArgument(request->args, "block-height"));
      response.args.insert(std::make_pair("block-height", std::to_string(height)));
      if (height < chain.size() && shard_id == std::to_string(shard_index)) {
        std::vector<byte> block = chain.raw_at(height);
        InputBuffer buffer(block);
        ChainState state;
        FinalBlock one_block(FinalBlock::Create(buffer, state));
        response.args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
        response.args.insert(std::make_pair("transactions", std::to_string(one_block.getSizeofTransactions())));
        response.args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
        response.args.insert(std::make_pair("block-volume", std::to_string(one_block.getVolume())));
        response.args.insert(std::make_pair("Merkle", ToHex(one_block.getMerkleRoot())));
        response.args.insert(std::make_pair("previous-hash", ToHex(one_block.getPreviousHash())));
	  } else if (hasBlock(shard_id, height, working_dir)) {
        std::vector<byte> block = ReadBlock(shard_id, height, working_dir);
        InputBuffer buffer(block);
        ChainState state;
        FinalBlock one_block(FinalBlock::Create(buffer, state));
        response.args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
        response.args.insert(std::make_pair("transactions", std::to_string(one_block.getNumTransactions())));
        response.args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
        response.args.insert(std::make_pair("block-volume", std::to_string(one_block.getVolume())));
        response.args.insert(std::make_pair("Merkle", ToHex(one_block.getMerkleRoot())));
        response.args.insert(std::make_pair("previous-hash", ToHex(one_block.getPreviousHash())));
      } else {
        response.return_code = 1050;
        response.message = "Block "+std::to_string(height)+" is not available.";
	  }
    } else if (SearchString(request->endpoint, "/shard-info", true)) {
      uint32_t highest = chain.size();
      response.args.insert(std::make_pair("current-block", std::to_string(highest)));
      if (highest > 0) {
        std::shared_ptr<FinalBlock> top_block = chain.back();
        response.args.insert(std::make_pair("last-block-time"
          , std::to_string(top_block->getBlockTime())));
        response.args.insert(std::make_pair("last-tx-count"
          , std::to_string(top_block->getNumTransactions())));
        size_t total_txs = chain.getNumTransactions();
        response.args.insert(std::make_pair("total-tx-count"
          , std::to_string(total_txs)));
        response.args.insert(std::make_pair("avg-tx-per-block"
          , std::to_string(total_txs/highest)));
        response.args.insert(std::make_pair("avg-block-time"
          , std::to_string(chain.getAvgBlocktime())));
	  }
    } else if (SearchString(request->endpoint, "/trace", true)) {
      std::string start_str = getServiceArgument(request->args, "start-block");
      size_t start_block = 0;
      if (!start_str.empty()) {
        start_block = std::stoi(start_str);
        response.args.insert(std::make_pair("start-block", start_str));
      }
      std::string end_str = getServiceArgument(request->args, "end-block");
      size_t end_block = UINT32_MAX;
      if (!end_str.empty()) {
        end_block = std::stoi(end_str);
        response.args.insert(std::make_pair("end-block", end_str));
      }
      std::string sig = getServiceArgument(request->args, "signature");
      std::string addr = getServiceArgument(request->args, "address");
      if (sig.empty() && addr.empty()) {
        response.return_code = 1050;
        response.message = "An address or signature is required for a trace.";
      }
      if (!sig.empty()) {
        response.args.insert(std::make_pair("signature", sig));
        std::vector<byte> target = Hex2Bin(sig);
        std::map<std::string, std::string> txs = TraceTransactions(shard_id
          , start_block, end_block, target, working_dir);
        response.args.insert(txs.begin(), txs.end());
      }
      if (!addr.empty()) {
        response.args.insert(std::make_pair("address", addr));
        std::vector<byte> target = Hex2Bin(addr);
        std::map<std::string, std::string> txs = TraceTransactions(shard_id
          , start_block, end_block, target, working_dir);
        response.args.insert(txs.begin(), txs.end());
	  }
    } else if (SearchString(request->endpoint, "/tx-info", true)) {
      size_t height = std::stoi(getServiceArgument(request->args, "block-height"));
      response.args.insert(std::make_pair("block-height", std::to_string(height)));
      std::string sig = getServiceArgument(request->args, "signature");
      response.args.insert(std::make_pair("signature", sig));
      Signature tx_id(Hex2Bin(sig));
      if (height < chain.size() && shard_id == std::to_string(shard_index)) {
        std::vector<byte> block = chain.raw_at(height);
        InputBuffer buffer(block);
        ChainState state;
        bool found_tx = false;
        FinalBlock one_block(FinalBlock::Create(buffer, state));
        for (const auto& tx : one_block.getTransactions()) {
          if (tx->getSignature() == tx_id) {
            response.args.insert(std::make_pair("tx-data", ToHex(tx->getCanonical())));
            found_tx = true;
            break;
          }
        }
        if (!found_tx) {
          response.return_code = 1050;
          response.message = "Transaction not found in block "+std::to_string(height)+".";
        }
      } else if (hasBlock(shard_id, height, working_dir)) {
        std::vector<byte> block = ReadBlock(shard_id, height, working_dir);
        InputBuffer buffer(block);
        ChainState state;
        bool found_tx = false;
        FinalBlock one_block(FinalBlock::Create(buffer, state));
        for (const auto& tx : one_block.getTransactions()) {
          if (tx->getSignature() == tx_id) {
            response.args.insert(std::make_pair("tx-data", ToHex(tx->getCanonical())));
            found_tx = true;
            break;
          }
        }
        if (!found_tx) {
          response.return_code = 1050;
          response.message = "Transaction not found in block "+std::to_string(height)+".";
        }
      } else {
        response.return_code = 1050;
        response.message = "Block "+std::to_string(height)+" is not available.";
	  }
    }
	return std::make_unique<ServiceResponse>(response);
  } catch (std::exception& e) {
    LOG_ERROR << "Devv-query Error: "+std::string(e.what());
    response.return_code = 1010;
    response.message = "Devv-query Error: "+std::string(e.what());
    return std::make_unique<ServiceResponse>(response);
  }
}
