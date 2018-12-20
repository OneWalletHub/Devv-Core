/*
 * KeyRing.h crypto key management for Devv.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef CONSENSUS_KEYRING_H_
#define CONSENSUS_KEYRING_H_

#include "common/devv_context.h"
#include "primitives/Transfer.h"

namespace Devv {


class KeyRing {
 public:
  KeyRing() : key_map_(), node_list_(), inn_addr_() {}
  KeyRing(const DevvContext& context);
  virtual ~KeyRing() {};

  /**
   * Add an address and EC key to this directory.
   * @param hex - a hexadecimal representation of this address.
   * @param key - a pointer to the openssl EC_KEY for this address
   * @return a binary representation of the provided address
   */
  Address InsertAddress(std::string hex, EC_KEY* key);
  /**
   * Adds wallet addresses to this directory from a file.
   * @param file_path - the path to the file containing wallets
   * @return true, iff wallets were loaded
   * @return false, if an error occurred or no wallets loaded
   */
  bool LoadWallets(const std::string& file_path, const std::string& file_pass);

  void addWalletKeyPair(const std::string& address, const std::string& key, const std::string& password) {
    std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
    Hash test_hash;
    test_hash = DevvHash(msg);

    EC_KEY* wallet_key = LoadEcKey(address, key, password);
    Signature sig = SignBinary(wallet_key, test_hash);

    if (!VerifyByteSig(wallet_key, test_hash, sig)) {
      throw std::runtime_error("Invalid address[" + address + "] key!");
    }

    Address wallet_addr = InsertAddress(address, wallet_key);
    wallet_list_.push_back(wallet_addr);
  }

  void addNodeKeyPair(const std::string& address, const std::string& key, const std::string& password) {
    std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
    Hash test_hash;
    test_hash = DevvHash(msg);

    EC_KEY* node_key = LoadEcKey(address, key, password);
    Signature sig = SignBinary(node_key, test_hash);

    if (!VerifyByteSig(node_key, test_hash, sig)) {
      throw std::runtime_error("Invalid node[" + address + "] key!");
    }

    Address wallet_addr = InsertAddress(address, node_key);
    node_list_.push_back(wallet_addr);
  }

  void setInnKeyPair(const std::string& address, const std::string& key, const std::string& password) {
    std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
    Hash test_hash;
    test_hash = DevvHash(msg);

    EC_KEY* inn_key = LoadEcKey(address, key, password);
    Signature sig = SignBinary(inn_key, test_hash);
    if (!VerifyByteSig(inn_key, test_hash, sig)) {
      LOG_FATAL << "Invalid INN key!";
      return;
    }

    inn_addr_ = InsertAddress(address, inn_key);
  }

  /**
   * Get a pointer to the EC key corresponding to an address.
   * @param addr - the Address to obtain the key for
   * @return a pointer to the openssl EC key for the provided address
   */
  EC_KEY* getKey(const Address& addr) const;
  /**
   * Check if the provided address belongs to the INN space
   * @param addr - the Address to check
   * @return true, iff this Address is an INN Address
   */
  bool isINN(const Address& addr) const;
  /**
   * @return the default INN Address
   */
  Address getInnAddr() const;
  /**
   * @return the number of node keys and addresses in this directory.
   */
  size_t CountNodes() const;
  /**
   * @return the number of wallet keys and addresses in this directory.
   */
  size_t CountWallets() const;
  /**
   * @param addr - the Address to check
   * @return the index of the provided node address
   * @return -1 if this Address is not a known node in this directory
   */
  unsigned int getNodeIndex(const Address& addr) const;
  /**
   * @param index - the index of the node to retrieve
   * @return the Address of the node at the provided index
   * @throw std::out_of_range if there is not a node at the given index
   */
  Address getNodeAddr(size_t index) const;
  /**
   * @param index - the index of the wallet to retrieve
   * @return the Address of the wallet at the provided index
   * @throw std::out_of_range if there is not a wallet at the given index
   */
  Address getWalletAddr(int index) const;
  /**
   * @param index - the index of the node to retrieve
   * @return the EC key of the node at the provided index
   * @throw std::out_of_range if there is not a node at the given index
   */
  EC_KEY* getNodeKey(int index) const;
  /**
   * @param index - the index of the wallet to retrieve
   * @return the EC key of the wallet at the provided index
   * @throw std::out_of_range if there is not a wallet at the given index
   */
  EC_KEY* getWalletKey(int index) const;
  /**
   * Retrieve the list of wallets managed by a particular node.
   * @param index - the index of the node to retrieve wallet addresses for
   * @return a vector of wallet Addresses managed by the node at the given index
   * @throw std::out_of_range if there is not a node at the given index
   */
  std::vector<Address> getDesignatedWallets(int index) const;

 private:
  KeyRing& operator=(const KeyRing&);
  KeyRing(const KeyRing&);

  std::map<Address, EC_KEY*> key_map_;
  std::vector<Address> node_list_;
  std::vector<Address> wallet_list_;
  Address inn_addr_;
};

} /* namespace Devv */

#endif /* CONSENSUS_KEYRING_H_ */
