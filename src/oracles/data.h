/*
 * data.h is an oracle to handle data storage on chain.
 *
 * Proposal.data format is addr|signature|data
 *
 * @copywrite  2018 Devvio Inc
 *
 */
#pragma once

#include <string>

#include "oracleInterface.h"
#include "consensus/chainstate.h"
#include "consensus/KeyRing.h"

namespace Devv {

class data : public oracleInterface {

 public:

  const int kBYTES_PER_COIN = 10240;

  data(std::string data) : oracleInterface(data) {};

  static Address getDataSinkAddress() {
    std::vector<byte> bin(kNODE_ADDR_SIZE, 0);
    Address sink(bin);
    return sink;
  }

/**
 *  @return the string name that invokes this oracle
 */
  virtual std::string getOracleName() override {
    return (data::GetOracleName());
  }

/**
 *  @return the string name that invokes this oracle
 */
  static std::string GetOracleName() {
    return ("io.devv.data");
  }

/**
 *  @return the shard used by this oracle
 */
  static uint64_t getShardIndex() {
    return (6);
  }

/**
 *  @return the coin type used by this oracle
 */
  static uint64_t getCoinIndex() {
    return (6);
  }

/** Checks if this proposal is objectively sound according to this oracle.
 *  When this function returns false, the proposal is syntactically unsound
 *  and will be invalid for all chain states.
 * @return true iff the proposal can be valid according to this oracle
 * @return false otherwise
 */
  bool isSound() override {
    size_t addr_size = Address::getSizeByType(raw_data_.at(0));
    if (addr_size < 1) {
      error_msg_ = "Bad address size.";
      return false;
    }
    size_t sig_size = Signature::getSizeByType(raw_data_.at(addr_size + 1));
    if (sig_size < 1) {
      error_msg_ = "Bad signature size.";
      return false;
    }
    size_t data_size = raw_data_.size() - addr_size - sig_size - 2;
    if (data_size <= 0) {
      error_msg_ = "Bad data size.";
      return false;
    }
    return true;
  }

/** Checks if this proposal is valid according to this oracle
 *  given a specific blockchain.
 * @params context the blockchain to check against
 * @return true iff the proposal is valid according to this oracle
 * @return false otherwise
 */
  bool isValid(const Blockchain& context) override {
    if (!isSound()) return false;
    size_t addr_size = Address::getSizeByType(raw_data_.at(0));
    Address client(Str2Bin(raw_data_.substr(0, addr_size + 1)));
    size_t sig_size = Signature::getSizeByType(raw_data_.at(addr_size + 1));
    size_t data_size = raw_data_.size() - addr_size - sig_size - 2;
    int64_t coins_needed = ceil(data_size / kBYTES_PER_COIN);
    ChainState last_state = context.getHighestChainState();
    int64_t available = last_state.getAmount(getCoinIndex(), client);
    if (available < coins_needed) {
      error_msg_ = "Data too large for coins provided";
      return false;
    }
    return true;
  }

/**
 *  @return if not valid or not sound, return an error message
 */
  std::string getErrorMessage() override {
    return (error_msg_);
  }

  std::map<uint64_t, std::vector<Tier2Transaction>>
  getTrace(const Blockchain& context) override {
    std::map<uint64_t, std::vector<Tier2Transaction>> out;
    return out;
  }

  uint64_t getCurrentDepth(const Blockchain& context) override {
    //@TODO(nick) scan pre-existing chain for this oracle instance.
    return (0);
  }

  std::map<uint64_t, std::vector<Tier2Transaction>>
  getNextTransactions(const Blockchain& context, const KeyRing& keys) override {
    std::map<uint64_t, std::vector<Tier2Transaction>> out;
    if (!isValid(context)) return out;
    size_t addr_size = Address::getSizeByType(raw_data_.at(0));
    Address client(Str2Bin(raw_data_.substr(0, addr_size + 1)));
    size_t sig_size = Signature::getSizeByType(raw_data_.at(addr_size + 1));
    Signature sig(Str2Bin(raw_data_.substr(addr_size + 1, sig_size + 1)));
    size_t data_size = raw_data_.size() - addr_size - sig_size - 2;
    int64_t coins_needed = ceil(data_size / kBYTES_PER_COIN);
    std::vector<Transfer> xfers;
    Transfer pay(client, getCoinIndex(), -1 * coins_needed, 0);
    Transfer settle(getDataSinkAddress(), getCoinIndex(), coins_needed, 0);
    xfers.push_back(pay);
    xfers.push_back(settle);
    std::vector<byte> nonce(Str2Bin(raw_data_.substr(raw_data_.size() - data_size)));
    Tier2Transaction the_tx(eOpType::Exchange, xfers, nonce, sig);
    std::vector<Tier2Transaction> txs;
    txs.push_back(std::move(the_tx));
    std::pair<uint64_t, std::vector<Tier2Transaction>> p(getShardIndex(), std::move(txs));
    out.insert(std::move(p));
    return out;
  }

/** Recursively generate the state of this oracle and all dependent oracles.
 *
 * @pre This proposal must be valid.
 * @params context the blockchain of the shard that provides context for this oracle
 * @return a map of oracles to data
 */
  std::map<std::string, std::vector<byte>>
  getDecompositionMap(const Blockchain& context) override {
    std::map<std::string, std::vector<byte>> out;
    std::vector<byte> data(Str2Bin(raw_data_));
    std::pair<std::string, std::vector<byte>> p(getOracleName(), data);
    out.insert(p);
    return out;
  }

/** Recursively generate the state of this oracle and all dependent oracles.
 *
 * @pre This proposal must be valid.
 * @params context the blockchain of the shard that provides context for this oracle
 * @return a map of oracles to data encoded in JSON
 */
  std::map<std::string, std::string>
  getDecompositionMapJSON(const Blockchain& context) override {
    std::map<std::string, std::string> out;
    std::pair<std::string, std::string> p(getOracleName(), getJSON());
    out.insert(p);
    return out;
  }

  std::vector<byte> Sign() override {
    return getCanonical();
  }

/**
 * @return the internal state of this oracle in JSON.
 */
  std::string getJSON() override {
    size_t addr_size = Address::getSizeByType(raw_data_.at(0));
    if (addr_size < 1) return "{\"data\":\"ADDR_ERROR\"}";
    Address client(Str2Bin(raw_data_.substr(0, addr_size + 1)));
    size_t sig_size = Signature::getSizeByType(raw_data_.at(addr_size + 1));
    if (sig_size < 1) return "{\"data\":\"SIG_ERROR\"}";
    Signature sig(Str2Bin(raw_data_.substr(addr_size + 1, sig_size + 1)));
    size_t data_size = raw_data_.size() - addr_size - sig_size - 2;
    std::string json("{\"addr\":\"" + client.getHexString() + "\",");
    json += "\"sig\":\"" + sig.getJSON() + "\",";
    json += "\"data\":\"" + ToHex(raw_data_.substr(raw_data_.size() - data_size)) + "\"}";
    return json;
  }

 private:
  std::string error_msg_;

};

} // namespace Devv
