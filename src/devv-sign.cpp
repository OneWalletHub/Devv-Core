/*
 * devv-sign.cpp signs a transaction using a given private key
 *
 * @copywrite  2018 Devvio Inc
 */

#include <cstdio>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "common/devv_context.h"
#include "io/file_ops.h"
#include "consensus/KeyRing.h"
#include "pbuf/devv_pbuf.h"
#include "common/ossladapter.h"

using namespace Devv;
namespace fs = boost::filesystem;

/**
 * Holds command-line options
 */
struct devvsign_options {
  enum eTxFileType {
    PROTOBUF = 0,
    JSON = 1,
    XML = 2,
    UNKNOWN = 3
  };

  fs::path envelope_file = "";
  fs::path key_file = "";
  std::string key_pass;
  bool quiet_mode = false;
  eTxFileType file_type = eTxFileType::UNKNOWN;
};

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct devvsign_options> ParseDevvsignOptions(int argc, char** argv);

void TestSign(EC_KEY& ec_key) {
  std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
  Hash test_hash;
  test_hash = DevvHash(msg);

  Signature sig = SignBinary(&ec_key, test_hash);
  if (!VerifyByteSig(&ec_key, test_hash, sig)) {
    throw std::runtime_error("VerifyByteSig failed");
  }
}

int main(int argc, char* argv[]) {
  init_log();

  std::unique_ptr<struct devvsign_options> options;
  try {
    options = ParseDevvsignOptions(argc, argv);

    if (!options) {
      exit(-1);
    }
  } catch (std::exception& e) {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    exit(-1);
  }

  auto& out_stream = options->quiet_mode ? std::cerr : std::cout;

  auto env = ReadBinaryFile(options->envelope_file);

  std::string env_str(env.begin(), env.end());

  auto tup = ReadKeyFile(options->key_file);
  out_stream << tup.address;
  out_stream << tup.key;

  options->file_type = devvsign_options::eTxFileType::PROTOBUF;

  // If there is a newline between the address and key, get rid of it
  if (tup.key[0] == '\n') {
    tup.key.erase(0,1);
  }

  auto ec_key = LoadEcKey(tup.address, tup.key, options->key_pass);

  if (ec_key == nullptr) {
    throw std::runtime_error("LoadEcKey failed");
  }
  try {
    TestSign(*ec_key);
  } catch (std::runtime_error& rte) {
    std::cerr << "TestSign failed";
  }

  KeyRing keys;
  keys.addNodeKeyPair(tup.address, tup.key, options->key_pass);

  out_stream << "addr_size    : " << tup.address.size() << std::endl;
  out_stream << "env_str_size : " << env_str.size() << std::endl;

  devv::proto::Envelope envelope;
  auto res = envelope.ParseFromString(env_str);

  out_stream << "res: " << res << std::endl;

  auto pb_transactions = envelope.txs();
  out_stream << "num tx: " << pb_transactions.size() << std::endl;
  for (auto const& transaction : pb_transactions) {
    auto tx = CreateTransaction(transaction, keys, true);
    out_stream << "Sig:" << std::endl;
    std::cout << tx->getSignature().getJSON() << std::endl;
  }
}

std::unique_ptr<struct devvsign_options> ParseDevvsignOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<devvsign_options> options(new devvsign_options());
  std::vector<std::string> config_filenames;

  po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Accepts a devv transaction as a json string and a file containing \n\
a private key. devvsign will sign the key and upon success, return\n\
a value of 0 and print the signature in hex format to stdout\n\
\nAllowed options");
  general.add_options()
      ("help,h", "produce help message")
      ("version,v", "print version string")
      ("config", po::value(&config_filenames), "Config file where options may be specified (can be specified more than once)")
      ("envelope-file,e", po::value<fs::path>(), "Envelope file")
      ("private-key,k", po::value<fs::path>(), "File containing private key to use in signing")
      ("key-pass", po::value<std::string>(), "Password for private keys")
      ("quiet-mode,q", "Runs in quiet mode: the signature is printed to stdout, all other output is directed to stderr")
      ;

  po::options_description all_options;
  all_options.add(general);

  po::variables_map vm;
  po::store(po::command_line_parser(argc, argv).
                options(all_options).
                run(),
                vm);

  if (vm.count("help")) {
    std::cout << all_options;
    return nullptr;
  }

  if (vm.count("envelope-file")) {
    options->envelope_file = vm["envelope-file"].as<fs::path>();
  } else {
    throw std::runtime_error("An Envelope file must be specified.");
  }

  if (vm.count("private-key")) {
    options->key_file = vm["private-key"].as<fs::path>();
  } else {
    throw std::runtime_error("Private key file was not set");
  }

  if (vm.count("key-pass")) {
    options->key_pass = vm["key-pass"].as<std::string>();
  } else {
    throw std::runtime_error("Key pass was not set.");
  }

  if (vm.count("quiet-mode")) {
    options->quiet_mode = true;
  } else {
    options->quiet_mode = false;
  }

  return options;
}
