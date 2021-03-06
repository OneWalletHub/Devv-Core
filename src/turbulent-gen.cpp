/*
 * turbulent-gen.cpp
 * Creates generate_count transactions as follows:
 * 1.  INN transactions create coins for every address
 * 2.  Each peer address attempts to send a random number of coins up to tx_amount
 *     to a random address other than itself
 * 3.  Many transactions will be invalid, but valid transactions should also appear indefinitely
 *
 * @copywrite  2018 Devvio Inc
 */

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "common/devv_context.h"
#include "modules/BlockchainModule.h"

using namespace Devv;

struct turbulent_options {
  eAppMode mode  = eAppMode::T1;
  unsigned int node_index = 0;
  unsigned int shard_index = 0;
  std::string write_file;
  std::string inn_keys;
  std::string node_keys;
  std::string wallet_keys;
  std::string key_pass;
  unsigned int generate_count;
  uint64_t tx_amount;
  eDebugMode debug_mode;
};

std::unique_ptr<struct turbulent_options> ParseTurbulentOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  CASH_TRY {
    auto options = ParseTurbulentOptions(argc, argv);

    if (!options) {
      LOG_ERROR << "ParseTurbulentOptions failed";
      exit(-1);
    }

    DevvContext this_context(options->node_index, options->shard_index, options->mode, options->inn_keys,
                                options->node_keys, options->key_pass);

    KeyRing keys(this_context);
    keys.LoadWallets(options->wallet_keys, options->key_pass);

    std::vector<byte> out;
    EVP_MD_CTX* ctx;
    if (!(ctx = EVP_MD_CTX_create())) {
      LOG_FATAL << "Could not create signature context!";
      CASH_THROW("Could not create signature context!");
    }

    Address inn_addr = keys.getInnAddr();

    size_t addr_count = keys.CountWallets();

    std::srand((unsigned)time(0));
    size_t counter = 0;

    std::vector<Transfer> xfers;
    Transfer inn_transfer(inn_addr, 0
      , -1l*addr_count*(addr_count-1)*options->tx_amount, 0);
    xfers.push_back(inn_transfer);
    for (size_t i = 0; i < addr_count; ++i) {
      Transfer transfer(keys.getWalletAddr(i), 0
        , (addr_count-1)*options->tx_amount, 0);
      xfers.push_back(transfer);
    }
    uint64_t nonce = GetMillisecondsSinceEpoch() + (1000000
                     * (options->node_index + 1) * (options->tx_amount + 1));
	std::vector<byte> nonce_bin;
    Uint64ToBin(nonce, nonce_bin);
    Tier2Transaction inn_tx(eOpType::Create, xfers, nonce_bin,
                            keys.getKey(inn_addr), keys);
    std::vector<byte> inn_canon(inn_tx.getCanonical());
    out.insert(out.end(), inn_canon.begin(), inn_canon.end());
    LOG_DEBUG << "GenerateTransactions(): generated inn_tx with sig: " << inn_tx.getSignature().getJSON();
    counter++;

    while (counter < options->generate_count) {
        for (size_t i = 0; i < addr_count; ++i) {
          size_t j = std::rand() % addr_count;
          size_t amount = std::rand() % options->tx_amount;
          if (i == j) { continue; }
          std::vector<Transfer> peer_xfers;
          Transfer sender(keys.getWalletAddr(i), 0, -1l * amount, 0);
          peer_xfers.push_back(sender);
          Transfer receiver(keys.getWalletAddr(j), 0, amount, 0);
          peer_xfers.push_back(receiver);
          nonce_bin.clear();
          nonce = GetMillisecondsSinceEpoch() + (1000000
                     * (options->node_index + 1) * (i + 1) * (j + 1));
          Uint64ToBin(nonce, nonce_bin);
          Tier2Transaction peer_tx(
              eOpType::Exchange, peer_xfers, nonce_bin,
              keys.getWalletKey(i), keys);
          std::vector<byte> peer_canon(peer_tx.getCanonical());
          out.insert(out.end(), peer_canon.begin(), peer_canon.end());
          LOG_TRACE << "GenerateTransactions(): generated tx with sig: " << peer_tx.getSignature().getJSON();
          counter++;
          if (counter >= options->generate_count) { break; }
        }  // end for loop
        if (counter >= options->generate_count) { break; }
    }  // end counter while

    LOG_INFO << "Generated " << counter << " transactions.";

    if (!options->write_file.empty()) {
      std::ofstream outFile(options->write_file, std::ios::out | std::ios::binary);
      if (outFile.is_open()) {
        outFile.write((const char*)out.data(), out.size());
        outFile.close();
      } else {
        LOG_FATAL << "Failed to open output file '" << options->write_file << "'.";
        return (false);
      }
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

std::unique_ptr<struct turbulent_options> ParseTurbulentOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<turbulent_options> options(new turbulent_options());

  try {
    po::options_description desc("\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Creates generate_count transactions as follows:\n\
1.  INN transactions create coins for every address\n\
2.  Each peer address attempts to send a random number of coins up to tx_amount\n\
    to a random address other than itself\n\
3.  Many transactions will be invalid, but valid transactions should also appear indefinitely\n\
\n\
Required parameters");
    desc.add_options()
        ("mode", po::value<std::string>(), "Devv mode (T1|T2|scan)")
        ("node-index", po::value<unsigned int>(), "Index of this node")
        ("shard-index", po::value<unsigned int>(), "Index of this shard")
        ("output", po::value<std::string>(), "Output path in binary JSON or CBOR")
        ("inn-keys", po::value<std::string>(), "Path to INN key file")
        ("node-keys", po::value<std::string>(), "Path to Node key file")
        ("wallet-keys", po::value<std::string>(), "Path to Wallet key file")
        ("key-pass", po::value<std::string>(), "Password for private keys")
        ("generate-tx", po::value<unsigned int>(), "Generate at least this many Transactions")
        ("tx-amount", po::value<uint64_t>(), "Number of coins to transfer in transaction")
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

    if (vm.count("output")) {
      options->write_file = vm["output"].as<std::string>();
      LOG_INFO << "Output file: " << options->write_file;
    } else {
      LOG_INFO << "Output file was not set.";
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

    if (vm.count("wallet-keys")) {
      options->wallet_keys = vm["wallet-keys"].as<std::string>();
      LOG_INFO << "Wallet keys file: " << options->wallet_keys;
    } else {
      LOG_INFO << "Wallet keys file was not set.";
    }

    if (vm.count("key-pass")) {
      options->key_pass = vm["key-pass"].as<std::string>();
      LOG_INFO << "Key pass: " << options->key_pass;
    } else {
      LOG_INFO << "Key pass was not set.";
    }

    if (vm.count("generate-tx")) {
      options->generate_count = vm["generate-tx"].as<unsigned int>();
      LOG_INFO << "Generate Transactions: " << options->generate_count;
    } else {
      LOG_INFO << "Generate Transactions was not set, defaulting to 0";
      options->generate_count = 0;
    }

    if (vm.count("tx-amount")) {
      options->tx_amount = vm["tx-amount"].as<uint64_t>();
      LOG_INFO << "Transaction amount: " << options->tx_amount;
    } else {
      options->tx_amount = 3210123;
      LOG_INFO << "Transaction amount was not set, defaulting to " << options->tx_amount;
    }
  }
  catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
