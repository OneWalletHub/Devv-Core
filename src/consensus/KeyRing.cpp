/*
 * KeyRing.cpp implements key management for Devcash.
 *
 *  Created on: Mar 3, 2018
 *      Author: Nick Williams
 */

#include "KeyRing.h"

#include <map>
#include <string>

namespace Devcash {

Address KeyRing::InsertAddress(std::string hex, EC_KEY* key) {
  std::vector<byte> addr(Hex2Bin(hex));
  Address to_insert(addr);
  std::pair<Address, EC_KEY*> new_pair(to_insert, key);
  key_map_.insert(new_pair);
  return to_insert;
}

KeyRing::KeyRing(const DevcashContext& context)
  : key_map_(), node_list_(), inn_addr_()
{
  CASH_TRY {
    EVP_MD_CTX* ctx;
    if (!(ctx = EVP_MD_CTX_create())) {
      LOG_FATAL << "Could not create signature context!";
      CASH_THROW("Could not create signature context!");
    }

    std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
    Hash test_hash;
    Signature sig;
    test_hash = DevcashHash(msg);

    std::string inn_keys;
    if (context.get_inn_key_path().size() > 0) {
      inn_keys = ReadFile(context.get_inn_key_path());
    }
    if (!inn_keys.empty()) {
      size_t size = inn_keys.size();
      if (size%(kFILE_KEY_SIZE+(kNODE_ADDR_SIZE*2)) == 0) {
        size_t counter = 0;
          while (counter < (size-1)) {
            std::string addr = inn_keys.substr(counter, (kNODE_ADDR_SIZE*2));
            counter += (kNODE_ADDR_SIZE*2);
            std::string key = inn_keys.substr(counter, kFILE_KEY_SIZE);
            counter += kFILE_KEY_SIZE;

            EC_KEY* inn_key = LoadEcKey(addr, key, context.get_key_password());
            SignBinary(inn_key, test_hash, sig);

            if (!VerifyByteSig(inn_key, test_hash, sig)) {
              LOG_FATAL << "Invalid INN key!";
              return;
            }

            inn_addr_ = InsertAddress(addr, inn_key);
         }
      } else {
        LOG_FATAL << "Invalid INN key file size ("+std::to_string(size)+")";
        return;
      }
    } else {
      EC_KEY* inn_key = LoadEcKey(context.kINN_ADDR,
          context.kINN_KEY, context.get_key_password());

      SignBinary(inn_key, test_hash, sig);

      if (!VerifyByteSig(inn_key, test_hash, sig)) {
        LOG_FATAL << "Invalid INN key!";
        return;
      }

      inn_addr_ = InsertAddress(context.kINN_ADDR, inn_key);
    }

    std::string node_keys;
    if (context.get_node_key_path().size() > 0)
    {
      node_keys = ReadFile(context.get_node_key_path());
    }
    if (!node_keys.empty()) {
      size_t size = node_keys.size();
      if (size%(kFILE_KEY_SIZE+(kNODE_ADDR_SIZE*2)) == 0) {
        size_t counter = 0;
          while (counter < (size-1)) {
            std::string addr = node_keys.substr(counter, (kNODE_ADDR_SIZE*2));
            counter += (kNODE_ADDR_SIZE*2);
            std::string key = node_keys.substr(counter, kFILE_KEY_SIZE);
            counter += kFILE_KEY_SIZE;

            EC_KEY* node_key = LoadEcKey(addr,key,context.get_key_password());
            SignBinary(node_key, test_hash, sig);

            if (!VerifyByteSig(node_key, test_hash, sig)) {
              LOG_WARNING << "Invalid node["+addr+"] key!";
            }

            Address node_addr = InsertAddress(addr, node_key);
            node_list_.push_back(node_addr);
         }
       } else {
         LOG_FATAL << "Invalid node key file size ("+std::to_string(size)+")";
         return;
       }

     } else {
       for (unsigned int i=0; i<context.kNODE_ADDRs.size(); i++) {
         EC_KEY* addr_key = LoadEcKey(context.kNODE_ADDRs[i],
             context.kNODE_KEYs[i], context.get_key_password());

         SignBinary(addr_key, test_hash, sig);
         if (!VerifyByteSig(addr_key, test_hash, sig)) {
           LOG_WARNING << "Invalid node["+std::to_string(i)+"] key!";
           CASH_THROW("Invalid node["+std::to_string(i)+"] key!");
         }

         Address node_addr = InsertAddress(context.kNODE_ADDRs[i], addr_key);
         node_list_.push_back(node_addr);
       }
     }

     LOG_INFO << "Crypto Keys initialized.";
   } CASH_CATCH (const std::exception& e) {
     LOG_WARNING << FormatException(&e, "transaction");
   }

}

bool KeyRing::LoadWallets(const std::string& file_path
                          , const std::string& file_pass) {
     EVP_MD_CTX* ctx;
     if (!(ctx = EVP_MD_CTX_create())) {
       LOG_FATAL << "Could not create signature context!";
       CASH_THROW("Could not create signature context!");
     }

     std::vector<byte> msg = {'h', 'e', 'l', 'l', 'o'};
     Hash test_hash;
     Signature sig;
     test_hash = DevcashHash(msg);

     std::string wallet_keys;
     if (file_path.size() > 0)
     {
       wallet_keys = ReadFile(file_path);
     }
     if (!wallet_keys.empty()) {
       size_t size = wallet_keys.size();
       if (size%(kFILE_KEY_SIZE+(kWALLET_ADDR_SIZE*2)) == 0) {
         size_t counter = 0;
           while (counter < (size-1)) {
             std::string addr = wallet_keys.substr(counter, (kWALLET_ADDR_SIZE*2));
             counter += (kWALLET_ADDR_SIZE*2);
             std::string key = wallet_keys.substr(counter, kFILE_KEY_SIZE);
             counter += kFILE_KEY_SIZE;

             EC_KEY* wallet_key = LoadEcKey(addr, key, file_pass);
             SignBinary(wallet_key, test_hash, sig);

             if (!VerifyByteSig(wallet_key, test_hash, sig)) {
               LOG_WARNING << "Invalid address["+addr+"] key!";
               continue;
             }

             Address wallet_addr = InsertAddress(addr, wallet_key);
             wallet_list_.push_back(wallet_addr);
          }
        } else {
          LOG_FATAL << "Invalid key file size ("+std::to_string(size)+")";
          return false;
        }
     } else {
       LOG_INFO << "No wallets found";
       return false;
	 }
     return true;
}

EC_KEY* KeyRing::getKey(const Address& addr) const {
  auto it = key_map_.find(addr);
  if (it != key_map_.end()) { return it->second; }

  //private key is not stored, so return public key only
  return LoadPublicKey(addr);
}

bool KeyRing::isINN(const Address& addr) const {
  return addr.isNodeAddress();
}

Address KeyRing::getInnAddr() const {
  return inn_addr_;
}

unsigned int KeyRing::CountNodes() const {
  return node_list_.size();
}

unsigned int KeyRing::CountWallets() const {
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

} /* namespace Devcash */
