/*
 * KeyRing.cpp implements crypto key management for Devv.
 *
 * @copywrite  2018 Devvio Inc
 */

#include "KeyRing.h"

#include <map>
#include <string>

namespace Devv {

Address KeyRing::InsertAddress(std::string hex, EC_KEY* key) {
  std::vector<byte> addr(Hex2Bin(hex));
  Address to_insert(addr);
  std::pair<Address, EC_KEY*> new_pair(to_insert, key);
  key_map_.insert(new_pair);
  return to_insert;
}

KeyRing::KeyRing(const DevvContext& context)
  : key_map_(), node_list_(), inn_addr_()
{
  CASH_TRY {
    EVP_MD_CTX* ctx;
    if (!(ctx = EVP_MD_CTX_create())) {
      LOG_FATAL << "Could not create signature context!";
      CASH_THROW("Could not create signature context!");
    }

    std::string inn_keys;
    if (context.get_inn_key_path().size() > 0) {
      inn_keys = ReadFile(context.get_inn_key_path());
    }
    if (!inn_keys.empty()) {
      size_t size = inn_keys.size();
      if (size%(kFILE_NODEKEY_SIZE+(kNODE_ADDR_SIZE*2)) == 0) {
        size_t counter = 0;
        while (counter < (size-1)) {
          std::string addr = inn_keys.substr(counter, kNODE_ADDR_SIZE*2);
          counter += (kNODE_ADDR_SIZE*2);
          std::string key = inn_keys.substr(counter, kFILE_NODEKEY_SIZE);
          counter += kFILE_NODEKEY_SIZE;
          try {
            setInnKeyPair(addr, key, context.get_key_password());
          } catch (const std::exception& e) {
            LOG_ERROR << FormatException(&e, "INN Key");
          }
        }
      } else {
        LOG_FATAL << "Invalid INN key file size ("+std::to_string(size)+")";
        return;
      }
    }

    std::string node_keys;
    if (context.get_node_key_path().size() > 0)
    {
      node_keys = ReadFile(context.get_node_key_path());
    }
    if (!node_keys.empty()) {
      size_t size = node_keys.size();
      if (size%(kFILE_NODEKEY_SIZE+(kNODE_ADDR_SIZE*2)) == 0) {
        size_t counter = 0;
          while (counter < (size-1)) {
          std::string addr = node_keys.substr(counter, (kNODE_ADDR_SIZE*2));
          counter += (kNODE_ADDR_SIZE*2);
          std::string key = node_keys.substr(counter, kFILE_NODEKEY_SIZE);
          counter += kFILE_NODEKEY_SIZE;
          try {
            addNodeKeyPair(addr,key,context.get_key_password());
          } catch (const std::exception& e) {
            LOG_ERROR << FormatException(&e, "Node Key");
          }
         }
       } else {
         LOG_FATAL << "Invalid node key file size ("+std::to_string(size)+")";
         return;
       }

     }

     LOG_INFO << "Crypto Keys initialized.";
   } CASH_CATCH (const std::exception& e) {
     LOG_WARNING << FormatException(&e, "transaction");
   }

}

bool KeyRing::LoadWallets(const std::string& file_path, const std::string& file_pass) {
  EVP_MD_CTX* ctx;
  if (!(ctx = EVP_MD_CTX_create())) {
    LOG_FATAL << "Could not create signature context!";
    CASH_THROW("Could not create signature context!");
  }

  std::string wallet_keys;
  if (file_path.size() > 0) {
    wallet_keys = ReadFile(file_path);
  }
  if (!wallet_keys.empty()) {
    size_t size = wallet_keys.size();
    if (size % (kFILE_KEY_SIZE + (kWALLET_ADDR_SIZE * 2)) == 0) {
      size_t counter = 0;
      while (counter < (size - 1)) {
        std::string addr = wallet_keys.substr(counter, (kWALLET_ADDR_SIZE * 2));
        counter += (kWALLET_ADDR_SIZE * 2);
        std::string key = wallet_keys.substr(counter, kFILE_KEY_SIZE);
        counter += kFILE_KEY_SIZE;
        try {
          addWalletKeyPair(addr, key, file_pass);
        } catch (const std::exception& e) {
          LOG_ERROR << FormatException(&e, "Wallet Key");
        }
      }
    } else {
      LOG_FATAL << "Invalid key file size (" + std::to_string(size) + ")";
      return false;
    }
  } else {
    LOG_INFO << "No wallets found";
    return false;
  }
  return true;
}

EC_KEY* KeyRing::getKey(const Address& addr) const {
  EC_KEY* eckey = nullptr;
  auto it = key_map_.find(addr);
  if (it != key_map_.end()) {
    eckey = EC_KEY_dup(it->second);
  } else {
    // private key is not stored, so return public key only
    eckey = LoadPublicKey(addr);
  }
  return eckey;
}

bool KeyRing::isINN(const Address& addr) const {
  return addr.isNodeAddress();
}

Address KeyRing::getInnAddr() const {
  if (isINN(inn_addr_)) {
    return inn_addr_;
  } else {
    throw std::runtime_error("INN Address is not initialized");
  }
}

size_t KeyRing::CountNodes() const {
  return node_list_.size();
}

size_t KeyRing::CountWallets() const {
  return wallet_list_.size();
}

/// FIXME @todo bug mckenney
unsigned int KeyRing::getNodeIndex(const Address& addr) const {
  unsigned int pos = find(node_list_.begin(), node_list_.end(), addr)
    - node_list_.begin();
  if (pos >= node_list_.size()) {
    throw std::range_error("Address not found in node_list_");
  }
  return pos;
}

Address KeyRing::getNodeAddr(size_t index) const {
  return node_list_.at(index);
}

Address KeyRing::getWalletAddr(int index) const {
  return wallet_list_.at(index);
}

EC_KEY* KeyRing::getNodeKey(int index) const {
  Address node_addr = node_list_.at(index);
  auto it = key_map_.find(node_addr);
  if (it != key_map_.end()) { return it->second; }
  LOG_WARNING << "Node["+std::to_string(index)+"] key is missing!\n";
  CASH_THROW("Node["+std::to_string(index)+"] key is missing!");
}

EC_KEY* KeyRing::getWalletKey(int index) const {
  Address wallet_addr = wallet_list_.at(index);
  auto it = key_map_.find(wallet_addr);
  if (it != key_map_.end()) { return it->second; }
  LOG_WARNING << "Wallet["+std::to_string(index)+"] key is missing!\n";
  CASH_THROW("Wallet["+std::to_string(index)+"] key is missing!");
}

std::vector<Address> KeyRing::getDesignatedWallets(int index) const {
  std::vector<Address> out;
  out.push_back(wallet_list_.at(index*2));
  out.push_back(wallet_list_.at(index*2+1));
  return out;
}

} /* namespace Devv */
