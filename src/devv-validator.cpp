/*
 * devv-validator.cpp the main class for a Devv core validator.
 *
 * @copywrite  2018 Devvio Inc
 */

#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <thread>

#include "common/argument_parser.h"
#include "common/devv_context.h"
#include "concurrency/ValidatorController.h"
#include "io/message_service.h"
#include "primitives/block_tools.h"
#include "modules/BlockchainModule.h"
#include "modules/ParallelExecutor.h"

#include <boost/filesystem.hpp>

using namespace Devv;
namespace fs = boost::filesystem;

Blockchain LoadPriorChain(ChainState& prior, const std::string& shard_dir
                         , const KeyRing& keys, eAppMode mode) {
  LOG_DEBUG << "Looking for prior blockchain at: " << shard_dir;
  Hash prev_hash = DevvHash({'G', 'e', 'n', 'e', 's', 'i', 's'});
  Blockchain the_chain(shard_dir);
  fs::path p(shard_dir);
  if (!p.empty() && is_directory(p)) {
    std::vector<std::string> files;
    std::set<Signature> dupe_check;
    for(auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
      files.push_back(entry.path().string());
    }
    std::sort(files.begin(), files.end());
    for  (auto const& file_name : files) {
      LOG_DEBUG << "Reading " << file_name;
      std::ifstream file(file_name, std::ios::binary);
      file.unsetf(std::ios::skipws);
      std::size_t file_size;
      file.seekg(0, std::ios::end);
      file_size = file.tellg();
      file.seekg(0, std::ios::beg);
      std::vector<byte> raw;
      raw.reserve(file_size);
      raw.insert(raw.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
      if (IsBlockData(raw)) {
        InputBuffer buffer(raw);
        while (buffer.getOffset() < static_cast<size_t>(file_size)) {
          try {
            auto new_block = std::make_shared<FinalBlock>(buffer, prior, keys, options->mode));
            Hash p_hash = new_block->getPreviousHash();
            if (!std::equal(std::begin(prev_hash), std::end(prev_hash), std::begin(p_hash))) {
              LOG_FATAL << "CHAINBREAK: The previous hash referenced in this block does not match the expected hash.";
              break;
            } else {
              prior = new_block->getChainState();
              prev_hash = DevvHash(new_block->getCanonical());
              the_chain.push_back(new_block);
	        }
          } catch (const std::exception& e) {
            LOG_ERROR << "Error scanning " << file_name << " skipping to next file.  Error details: "+FormatException(&e, "validator.init");
            break;
          }
        }
	  } else {
        LOG_WARNING << "Working directory contained non-block binary data at: " << file_name;
	  }
  }
  return(priori);
}

int main(int argc, char* argv[])
{

  try {
    auto options = ParseDevvOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    LoggerContext lg_context(options->syslog_host, options->syslog_port);

    init_log(lg_context);

    zmq::context_t zmq_context(1);

    DevvContext devv_context(options->node_index
                                , options->shard_index
                                , options->mode
                                , options->inn_keys
                                , options->node_keys
                                , options->key_pass
                                , options->batch_size
                                , options->max_wait);
    KeyRing keys(devv_context);
    ChainState prior;

    //load existing blockchain, modifies prior ChainState
    Blockchain the_chain = LoadPriorChain(prior, options->working_dir, keys, options->mode);

    auto server = io::CreateTransactionServer(options->bind_endpoint, zmq_context);
    auto peer_client = io::CreateTransactionClient(options->host_vector, zmq_context);


    // Create loopback client to subscribe to simulator transactions
    std::unique_ptr<io::TransactionClient> loopback_client(new io::TransactionClient(zmq_context));
    auto be = options->bind_endpoint;
    std::string this_uri;
    try {
      this_uri = "tcp://localhost" + be.substr(be.rfind(':'));
    } catch (std::range_error& e) {
      LOG_ERROR << "Extracting bind number failed: " << be;
    }
    loopback_client->addConnection(this_uri);


    /**
     * Chrome tracing setup
     */
    if (options->trace_file.empty()) {
      LOG_FATAL << "Trace file is required.";
      exit(-1);
	}
    mtr_init(options->trace_file.c_str());
    mtr_register_sigint_handler();

    MTR_META_PROCESS_NAME("minitrace_test");
    MTR_META_THREAD_NAME("main thread");

    MTR_BEGIN("main", "outer");

    {
      LOG_NOTICE << "Creating the BlockchainModule";
      auto bcm = BlockchainModule::Create(*server, *peer_client, *loopback_client
                 , the_chain, keys, prior
                 , options->mode, devv_context, options->batch_size);
      LOG_NOTICE << "Starting the BlockchainModule";

      bcm->start();

      for (;;) {
        int i = 10;
        LOG_DEBUG << "main loop: sleeping " << i;
        sleep(i);
      }
      LOG_NOTICE << "Stopping the BlockchainModule";
    }

    LOG_NOTICE << "BlockchainModule is halted.";

    MTR_END("main", "outer");
    mtr_flush();
    mtr_shutdown();

    LOG_INFO << "Devv Shutting Down";

    return(true);
  } catch (...) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: "+err <<  std::endl;
    std::cerr << err << std::endl;
    return(false);
  }
}


/*
    std::signal(SIGINT, signal_handler);
    std::signal(SIGABRT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    shutdown_handler = [&](int signal) {
      LOG_INFO << "Received signal ("+std::to_string(signal)+").";
      shutdown();
    };
 */

