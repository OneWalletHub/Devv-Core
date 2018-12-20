/*
 * devv-key.cpp generates a public/private keypair
 *
 * @copywrite  2018 Devvio Inc
 */

#include <cstdio>
#include <iostream>

#include <boost/filesystem.hpp>
#include <boost/program_options.hpp>

#include "common/ossladapter.h"

using namespace Devv;
namespace fs = boost::filesystem;

/**
 * Holds command-line options
 */
struct devvkey_options {
  fs::path key_file = "";
  std::string key_pass;
};

/**
 * Parse command-line options
 * @param argc
 * @param argv
 * @return
 */
std::unique_ptr<struct devvkey_options> ParseDevvkeyOptions(int argc, char** argv);

int main(int argc, char* argv[]) {

  std::unique_ptr<struct devvkey_options> options;
  try {
    options = ParseDevvkeyOptions(argc, argv);

    if (!options) {
      exit(-1);
    }
  } catch (std::exception& e) {
    std::cerr << "Error parsing options: " << e.what() << std::endl;
    exit(-1);
  }

  GenerateAndWriteKeyfile(options->key_file.string(), 1, options->key_pass);
  exit(0);
}

std::unique_ptr<struct devvkey_options> ParseDevvkeyOptions(int argc, char** argv) {

  namespace po = boost::program_options;

  std::unique_ptr<devvkey_options> options(new devvkey_options());
  std::vector<std::string> config_filenames;

  po::options_description general("General Options\n\
" + std::string(argv[0]) + " [OPTIONS] \n\
\n\
Generates a wallet address and corresponding encrypted private key\n\
\nAllowed options");
  general.add_options()
      ("help,h", "produce help message")
      ("version,v", "print version string")
      ("key-file,k", po::value<fs::path>(), "File to write key pair")
      ("key-pass,p", po::value<std::string>(), "Password for private keys")
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

  if (vm.count("key-file")) {
    options->key_file = vm["key-file"].as<fs::path>();
  } else {
    throw std::runtime_error("Private key file was not set");
  }

  if (vm.count("key-pass")) {
    options->key_pass = vm["key-pass"].as<std::string>();
  } else {
    throw std::runtime_error("Key pass was not set.");
  }

  return options;
}
