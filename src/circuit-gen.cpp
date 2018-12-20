/*
 * circuit-gen.cpp
 * Creates up to generate_count transactions as follows:
 * 1.  INN transactions create addr_count coins for every address
 * 2.  Each peer address sends 1 coin to every other address
 * 3.  Each peer address returns addr_count coins to the INN
 *
 *  For perfect circuits,
 *    make sure that generate_count is one more than a perfect square.
 *
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <memory>

#include <boost/program_options.hpp>

#include "common/devv_context.h"
#include "common/logger.h"

#include "modules/BlockchainModule.h"

using namespace Devv;

struct circuit_options {
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
  bool clear_state;
};

std::unique_ptr<struct circuit_options> ParseCircuitOptions(int argc, char** argv);

int main(int argc, char* argv[]) {
  init_log();

  CASH_TRY {
    std::unique_ptr<circuit_options> options = ParseCircuitOptions(argc, argv);

    if (!options) {
      exit(-1);
    }

    DevvContext this_context(options->node_index, options->shard_index, options->mode, options->inn_keys,
                                options->node_keys, options->key_pass);

    KeyRing keys(this_context);
    keys.LoadWallets(options->wallet_keys, options->key_pass);

    std::vector<byte> out;
    EVP_MD_CTX* ctx;
    if (!(ctx = EVP_MD_CTX_create())) {
      std::string err("Could not create signature context!");
      throw std::runtime_error(err);
    }

    Address inn_addr = keys.getInnAddr();

    if (options->generate_count < 2) {
      LOG_FATAL << "Must generate at least 2 transactions for a complete circuit.";
      CASH_THROW("Invalid number of transactions to generate.");
    }
    // Need sqrt(N-1) addresses (x) to create N circuits: 1+x(x-1)+2x=N
    double need_addrs = std::sqrt(options->generate_count - 1);
    // if sqrt(N-1) is not an int, circuits will be incomplete
    if (std::floor(need_addrs) != need_addrs) {
      LOG_WARNING << "For complete circuits generate a perfect square + 1 transactions (ie 2,5,10,17...)";
    }

    size_t addr_count = std::min(keys.CountWallets(), static_cast<size_t>(need_addrs));

    size_t counter = 0;

    std::vector<Transfer> xfers;
    Transfer inn_transfer(inn_addr, 0, -1l * addr_count * (addr_count - 1) * options->tx_amount, 0);
    xfers.push_back(inn_transfer);
    for (size_t i = 0; i < addr_count; ++i) {
      Transfer transfer(keys.getWalletAddr(i), 0, options->tx_amount * (addr_count - 1), 0);
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
    LOG_DEBUG << "Circuit test generated inn_tx with sig: " << inn_tx.getSignature().getJSON();
    counter++;

    while (counter < options->generate_count) {
      for (size_t i = 0; i < addr_count; ++i) {
        for (size_t j = 0; j < addr_count; ++j) {
          if (i == j) { continue; }
          std::vector<Transfer> peer_xfers;
          Transfer sender(keys.getWalletAddr(i), 0, -1l * options->tx_amount, 0);
          peer_xfers.push_back(sender);
          Transfer receiver(keys.getWalletAddr(j), 0, options->tx_amount, 0);
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
          LOG_TRACE << "Circuit test generated tx with sig: " << peer_tx.getSignature().getJSON();
          counter++;
          if (counter >= options->generate_count) { break; }
        }  // end inner for
        if (counter >= options->generate_count) { break; }
      }  // end outer for
      if (counter >= options->generate_count) { break; }
      if (options->clear_state) {
        for (size_t i = 0; i < addr_count; ++i) {
          std::vector<Transfer> peer_xfers;
          Transfer sender(keys.getWalletAddr(i), 0, -1l * (addr_count - 1) * options->tx_amount, 0);
          peer_xfers.push_back(sender);
          Transfer receiver(inn_addr, 0, (addr_count - 1) * options->tx_amount, 0);
          peer_xfers.push_back(receiver);
          nonce = GetMillisecondsSinceEpoch() + (1000000
                       * (options->node_index + 1) * (i + 1) * (addr_count + 2));
          Uint64ToBin(nonce, nonce_bin);
          Tier2Transaction peer_tx(
              eOpType::Exchange, peer_xfers, nonce_bin,
              keys.getWalletKey(i), keys);
          std::vector<byte> peer_canon(peer_tx.getCanonical());
          out.insert(out.end(), peer_canon.begin(), peer_canon.end());
          LOG_TRACE << "GenerateTransactions(): generated tx with sig: " << peer_tx.getSignature().getJSON();
          counter++;
          if (counter >= options->generate_count) { break; }
        }  // end outer for
      }
      if (counter >= options->generate_count) { break; }
    }  // end counter while

    LOG_INFO << "Generated " << counter << " transactions.";

    if (!options->write_file.empty()) {
      std::ofstream out_file(options->write_file, std::ios::out | std::ios::binary);
      if (out_file.is_open()) {
        out_file.write((const char*) out.data(), out.size());
        out_file.close();
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

std::unique_ptr<struct circuit_options> ParseCircuitOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<circuit_options> options(new circuit_options());

  try {
    po::options_description desc("\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Creates up to generate_count transactions as follows:\n\
 1.  INN transactions create addr_count coins for every address\n\
 2.  Each peer address sends 1 coin to every other address\n\
 3.  Each peer address returns addr_count coins to the INN\n\
\n\
For perfect circuits, make sure that generate_count is one more \n\
than a perfect square.\n\
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
        ("clear-state", po::value<bool>(), "Return coins to INN address?")
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

    if (vm.count("clear-state")) {
      options->clear_state = vm["clear-state"].as<bool>();
      LOG_INFO << "Clear state: " << options->clear_state;
    } else {
      options->clear_state = true;
      LOG_INFO << "Clear state was not set, defaulting to " << options->clear_state;
    }
  }
  catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
