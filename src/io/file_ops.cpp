/*
 * file_ops.cpp structure to read files associated with Devv.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "file_ops.h"

#include "common/devv_constants.h"

namespace Devv {

std::vector<byte> ReadBinaryFile(const fs::path& path) {
  // open the file:
  std::ifstream file(path.string(), std::ios::binary);

  // Stop eating new lines in binary mode!!!
  file.unsetf(std::ios::skipws);

  // get the size:
  std::streampos file_size;

  file.seekg(0, std::ios::end);
  file_size = file.tellg();
  file.seekg(0, std::ios::beg);

  // reserve capacity
  std::vector<byte> vec;
  vec.reserve(file_size);

  // read the data:
  vec.insert(vec.begin(),
             std::istream_iterator<byte>(file),
             std::istream_iterator<byte>());

  return vec;
}

std::string ReadTextFile(const fs::path& file_path)
{
  std::ifstream file(file_path.string());
  if (!file.good()) {
    throw std::runtime_error("File " + file_path.string() + " could not be found");
  }

  if (!file.is_open()) {
    throw std::runtime_error("File " + file_path.string() + " could not be opened, check permissions");
  }

  std::string output;
  for(std::string line; std::getline(file, line);) {
    output += line+"\n";
  }
  return(output);
}

struct key_tuple ReadKeyFile(const fs::path& path) {
  std::string key_string = ReadTextFile(path);
  std::string key_header = "-----BEGIN ENCRYPTED PRIVATE KEY-----";

  auto pos = key_string.find(key_header);

  struct key_tuple tuple = {"", ""};

  auto node_addr_bytes = kNODE_ADDR_SIZE * 2;
  auto wallet_addr_bytes = kWALLET_ADDR_SIZE * 2;

  unsigned int file_key_size = kFILE_KEY_SIZE;
  size_t ossl10size = wallet_addr_bytes + file_key_size + 1;
  size_t ossl11size = ossl10size + 20;

  if (key_string.size() == ossl10size) {
  } else if (key_string.size() == ossl11size) {
    file_key_size = file_key_size + 20;
  } else {
    std::string err = "The key file size is not supported: "+std::to_string(key_string.size());
    throw std::runtime_error(err);
  }

  // Make the position tolerant to newline at end of address line
  if (pos == node_addr_bytes) {
    tuple.address = key_string.substr(0, node_addr_bytes);
    tuple.key = key_string.substr(node_addr_bytes, kFILE_NODEKEY_SIZE);
  } else if (pos == wallet_addr_bytes) {
    tuple.address = key_string.substr(0, wallet_addr_bytes);
    tuple.key = key_string.substr(wallet_addr_bytes, kFILE_KEY_SIZE);
  } else if (pos == (node_addr_bytes+1)) {
    tuple.address = key_string.substr(0, node_addr_bytes);
    tuple.key = key_string.substr(node_addr_bytes+1, kFILE_NODEKEY_SIZE);
  } else if (pos == (wallet_addr_bytes+1)) {
    tuple.address = key_string.substr(0, wallet_addr_bytes);
    tuple.key = key_string.substr(wallet_addr_bytes+1, file_key_size);
  } else {
    std::string err("Malformed key file: ");
    throw std::runtime_error(err + std::to_string(pos) +
        "node_addr_bytes(" + std::to_string(node_addr_bytes) +
        ") wallet_addr_bytes(" + std::to_string(wallet_addr_bytes));
  }
  return tuple;
}

} // namespace Devv
