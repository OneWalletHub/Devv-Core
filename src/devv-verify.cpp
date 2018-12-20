
/*
 * devv-verify.cpp scans a directory for transactions and blocks.
 * Then, it accumulates the states implied by all the sound transactions it finds
 * as well as getting the chain state implied by the blockchain(s) in the directory.
 * Finally, it compares these states and dumps them if they are not equal.
 *
 * If states are not equal, keep in mind that order and timing matter.
 * A transaction may be sound, but not valid when it is provided to the verifier.
 * In this case, this transaction will be recorded in the chainstate implied by sound transactions,
 * but not the chainstate created in the actual chains and the chains may be correct even though
 * they are missing that transaction, which could not be processed when it was announced.
 *
 * Likewise, this verion is only designed to verify 1 chain.
 * If there are transactions being distributed among multiple chains, the chains may arrive at
 * different states from the one implied by the set of transactions themselves.
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

#include "common/devv_context.h"
#include "io/message_service.h"
#include "modules/BlockchainModule.h"
#include "primitives/json_interface.h"
#include "primitives/block_tools.h"

using namespace Devv;

namespace fs = boost::filesystem;

struct verify_options {
  eAppMode mode  = eAppMode::T1;
  unsigned int node_index = 0;
  unsigned int shard_index = 0;
  std::string working_dir;
  std::string inn_keys;
  std::string node_keys;
  std::string key_pass;
  eDebugMode debug_mode;
};

std::unique_ptr<struct verify_options> ParseVerifyOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  CASH_TRY {
    auto options = ParseVerifyOptions(argc, argv);

    if (!options) {
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

    LOG_DEBUG << "Verifying " << options->working_dir;
    MTR_SCOPE_FUNC();

    ChainState priori;
    ChainState posteri;
    Summary summary = Summary::Create();

    fs::path p(options->working_dir);

    if (!is_directory(p)) {
      LOG_ERROR << "Error opening dir: " << options->working_dir << " is not a directory";
      return false;
    }

    std::vector<std::string> files;

    for (auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
      files.push_back(entry.path().string());
    }

    for (auto const& file_name : files) {
      LOG_INFO << "Reading " << file_name;
      std::ifstream file(file_name, std::ios::binary);
      file.unsetf(std::ios::skipws);
      std::streampos file_size;
      file.seekg(0, std::ios::end);
      file_size = file.tellg();
      file.seekg(0, std::ios::beg);

      std::vector<byte> raw;
      raw.reserve(file_size);
      raw.insert(raw.begin(), std::istream_iterator<byte>(file), std::istream_iterator<byte>());
      assert(file_size > 0);
      bool is_block = IsBlockData(raw);
      bool is_transaction = IsTxData(raw);
      if (is_block) { LOG_INFO << file_name << " has blocks."; }
      if (is_transaction) { LOG_INFO << file_name << " has transactions."; }
      if (!is_block && !is_transaction) { LOG_WARNING << file_name << " contains unknown data."; }

      InputBuffer buffer(raw);
      while (buffer.getOffset() < static_cast<size_t>(file_size)) {
		try {
          if (is_block) {
            FinalBlock one_block(buffer, posteri, keys, options->mode);
            Summary block_summary(Summary::Create());
            std::vector<TransactionPtr> txs = one_block.CopyTransactions();
            for (TransactionPtr& item : txs) {
              if (!item->isValid(posteri, keys, block_summary)) {
                LOG_WARNING << "A transaction is invalid. TX details: ";
                LOG_WARNING << item->getJSON();
              }
            }
            if (block_summary.getCanonical() != one_block.getSummary().getCanonical()) {
              LOG_WARNING << "A final block summary is invalid. Summary datails: ";
              LOG_WARNING << GetJSON(one_block.getSummary());
              LOG_WARNING << "Transaction details: ";
              for (TransactionPtr& item : txs) {
                LOG_WARNING << item->getJSON();
              }
            }
          } else if (is_transaction) {
            Tier2Transaction tx(Tier2Transaction::Create(buffer, keys, true));
            if (!tx.isValid(priori, keys, summary)) {
              LOG_WARNING << "A transaction is invalid. TX details: ";
              LOG_WARNING << tx.getJSON();
            }
          } else {
            LOG_WARNING << "!is_block && !is_transaction";
          }
        } catch (const std::exception& e) {
          LOG_ERROR << "Error in " << file_name << " skipping to next file.  Error details: "+FormatException(&e, "scanner");
          LOG_ERROR << "Offset/Size: "+std::to_string(buffer.getOffset())+"/"+std::to_string(file_size);
          break;
        }
      }
    }

    if (!CompareChainStateMaps(priori.getStateMap(), posteri.getStateMap())) {
      LOG_WARNING << "End states do not match!";
      LOG_WARNING << "Prior state: " + WriteChainStateMap(priori.getStateMap());
      LOG_WARNING << "Post state: " + WriteChainStateMap(posteri.getStateMap());
    } else {
      LOG_INFO << "End states match.";
    }

    if (posteri.getStateMap().empty()) {
      LOG_INFO << "End state is empty.";
    }

    return (true);
  }
  CASH_CATCH(...) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: " + err << std::endl;
    std::cerr << err << std::endl;
    return (false);
  }
}

std::unique_ptr<struct verify_options> ParseVerifyOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  auto options = std::make_unique<verify_options>();

  try {
    po::options_description desc("\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Scans a directory for transactions and blocks.\n\
Then, it accumulates the states implied by all the sound transactions it finds\n\
as well as getting the chain state implied by the blockchain(s) in the directory.\n\
Finally, it compares these states and dumps them if they are not equal.\n\
\n\
If states are not equal, keep in mind that order and timing matter.\n\
A transaction may be sound, but not valid when it is provided to the verifier.\n\
In this case, this transaction will be recorded in the chainstate implied by sound transactions,\n\
but not the chainstate created in the actual chains and the chains may be correct even though\n\
they are missing that transaction, which could not be processed when it was announced.\n\
\n\
Likewise, this verion is only designed to verify 1 chain.\n\
If there are transactions being distributed among multiple chains, the chains may arrive at\n\
different states from the one implied by the set of transactions themselves.\n\
\n\
Required parameters");
    desc.add_options()
        ("mode", po::value<std::string>(), "Devv mode (T1|T2|scan)")
        ("node-index", po::value<unsigned int>(), "Index of this node")
        ("shard-index", po::value<unsigned int>(), "Index of this shard")
        ("working-dir", po::value<std::string>(), "Directory to be verified")
        ("inn-keys", po::value<std::string>(), "Path to INN key file")
        ("node-keys", po::value<std::string>(), "Path to Node key file")
        ("key-pass", po::value<std::string>(), "Password for private keys")
        ;

    po::options_description d2("Optional parameters");
    d2.add_options()
        ("help", "produce help message")
        ("debug-mode", po::value<std::string>(), "Debug mode (on|off|perf) for testing")
        ;
    desc.add(d2);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      std::cout << desc << "\n";
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
      } else if (debug_mode == "perf") {
        options->debug_mode = perf;
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
  }
  catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
