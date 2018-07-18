/*
 * scanner.cpp the main class for a block scanner.
 * @note the mode must be set appropriately
 * Use T1 to scan a tier 1 chain, T2 to scan a T2 chain, and scan to scan raw transactions.
 *
 * Scans and checks public keys without using any private keys.
 * Handles each file independently, so blockchains should be in a single file.
 *
 *  Created on: May 20, 2018
 *  Author: Nick Williams
 */

#include <string>
#include <iostream>
#include <fstream>
#include <functional>
#include <thread>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "common/devcash_context.h"
#include "modules/BlockchainModule.h"
#include "primitives/json_interface.h"
#include "primitives/block_tools.h"

using namespace Devcash;
namespace fs = boost::filesystem;

struct scanner_options {
  eAppMode mode  = eAppMode::T1;
  unsigned int node_index = 0;
  unsigned int shard_index = 0;
  std::string working_dir;
  std::string write_file;
  std::string inn_keys;
  std::string node_keys;
  std::string key_pass;
  unsigned int generate_count;
  unsigned int tx_limit;
  eDebugMode debug_mode;
};

std::unique_ptr<struct scanner_options> ParseScannerOptions(int argc, char** argv);

int main(int argc, char* argv[])
{
  init_log();

  CASH_TRY {
    auto options = ParseScannerOptions(argc, argv);

    if (!options) {
      LOG_ERROR << "ParseScannerOptions failed";
      exit(-1);
    }

    size_t block_counter = 0;
    size_t tx_counter = 0;
    size_t tfer_count = 0;

    KeyRing keys;

    fs::path p(options->working_dir);
    if (p.empty()) {
      LOG_WARNING << "Invalid path: "+options->working_dir;
      return(false);
    }

    if (!is_directory(p)) {
      LOG_ERROR << "Error opening dir: " << options->working_dir << " is not a directory";
      return false;
    }

    std::string out;
    std::vector<std::string> files;
    for(auto& entry : boost::make_iterator_range(fs::directory_iterator(p), {})) {
      files.push_back(entry.path().string());
    }
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
      bool is_block = IsBlockData(raw);
      bool is_transaction = IsTxData(raw);
      if (is_block) { LOG_INFO << file_name << " has blocks."; }
      if (is_transaction) { LOG_INFO << file_name << " has transactions."; }
      if (!is_block && !is_transaction) {
        LOG_WARNING << file_name << " contains unknown data.";
      }
      size_t file_blocks = 0;
      size_t file_txs = 0;
      size_t file_tfer = 0;

      ChainState priori;
      ChainState posteri;

      InputBuffer buffer(raw);
      while (buffer.getOffset() < static_cast<size_t>(file_size)) {
        if (is_transaction) {
          Tier2Transaction tx(Tier2Transaction::Create(buffer, keys, true));
          file_txs++;
          file_tfer += tx.getTransfers().size();
          out += tx.getJSON();
        } else if (is_block) {
          size_t span = buffer.getOffset();
          FinalBlock one_block(buffer, priori, keys, options->mode);
          if (buffer.getOffset() == span) {
            LOG_WARNING << file_name << " has invalid block!";
            break;
		  }
          size_t txs = one_block.getNumTransactions();
          size_t tfers = one_block.getNumTransfers();
          priori = one_block.getChainState();
          out += GetJSON(one_block);

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

          out += std::to_string(txs)+" txs, transfers: "+std::to_string(tfers);
          LOG_INFO << std::to_string(txs)+" txs, transfers: "+std::to_string(tfers);
          file_blocks++;
          file_txs += txs;
          file_tfer += tfers;
        } else {
          LOG_WARNING << "!is_block && !is_transaction";
        }
      }

      out += "End chainstate: " + WriteChainStateMap(posteri.getStateMap());

      out += file_name + " has " + std::to_string(file_txs)
              + " txs, " +std::to_string(file_tfer)+" tfers in "+std::to_string(file_blocks)+" blocks.";
      LOG_INFO << file_name << " has " << std::to_string(file_txs)
              << " txs, " +std::to_string(file_tfer)+" tfers in "+std::to_string(file_blocks)+" blocks.";
      block_counter += file_blocks;
      tx_counter += file_txs;
      tfer_count += file_tfer;
    }
    out += "Dir has "+std::to_string(tx_counter)+" txs, "
      +std::to_string(tfer_count)+" tfers in "+std::to_string(block_counter)+" blocks.";
    LOG_INFO << "Dir has "+std::to_string(tx_counter)+" txs, "
      +std::to_string(tfer_count)+" tfers in "+std::to_string(block_counter)+" blocks.";

    if (!options->write_file.empty()) {
      std::ofstream out_file(options->write_file, std::ios::out);
      if (out_file.is_open()) {
        out_file << out;
        out_file.close();
      } else {
        LOG_FATAL << "Failed to open output file '" << options->write_file << "'.";
        return(false);
      }
    }

    return(true);
  } CASH_CATCH (...) {
    std::exception_ptr p = std::current_exception();
    std::string err("");
    err += (p ? p.__cxa_exception_type()->name() : "null");
    LOG_FATAL << "Error: "+err <<  std::endl;
    std::cerr << err << std::endl;
    return(false);
  }
}

std::unique_ptr<struct scanner_options> ParseScannerOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  auto options = std::make_unique<scanner_options>();

  try {
    po::options_description desc("\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
A block scanner.\n\
Use T1 to scan a tier 1 chain, T2 to scan a T2 chain, and scan to\n\
scan raw transactions.\n\
\n\
Required parameters");
    desc.add_options()
        ("mode", po::value<std::string>(), "Devcash mode (T1|T2|scan)")
        ("working-dir", po::value<std::string>(), "Directory where inputs are read and outputs are written")
        ("output", po::value<std::string>(), "Output path in binary JSON or CBOR")
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

    if (vm.count("working-dir")) {
      options->working_dir = vm["working-dir"].as<std::string>();
      LOG_INFO << "Working dir: " << options->working_dir;
    } else {
      LOG_INFO << "Working dir was not set.";
    }

    if (vm.count("output")) {
      options->write_file = vm["output"].as<std::string>();
      LOG_INFO << "Output file: " << options->write_file;
    } else {
      LOG_INFO << "Output file was not set.";
    }
  }
  catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what();
    return nullptr;
  }

  return options;
}
