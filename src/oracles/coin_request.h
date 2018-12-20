/*
 * coin_request.h is a testnet oracle to request coins from the INN.
 *
 * Proposal format is coin|amount|addr
 * Amount must be positive.
 * Coin type must be Devv.
 *
 * @copywrite  2018 Devvio Inc
 *
 */
#pragma once

#include <string>

#include "oracles/oracleInterface.h"
#include "consensus/chainstate.h"
#include "consensus/KeyRing.h"

namespace Devv {

class CoinRequest : public oracleInterface {

 public:


CoinRequest(std::string data) : oracleInterface(data) {};

/**
 *  @return the string name that invokes this oracle
 */
  virtual std::string getOracleName() override {
    return(CoinRequest::GetOracleName());
  }

/**
 *  @return the string name that invokes this oracle
 */
  static std::string GetOracleName() {
    return("io.devv.coin_request");
  }

/**
 *  @return the shard used by this oracle
 */
  static uint64_t getShardIndex() {
    return(1);
  }

/**
 *  @return the coin type used by this oracle
 */
  static uint64_t getCoinIndex() {
    return(0);
  }

/** Checks if this proposal is objectively sound according to this oracle.
 *  When this function returns false, the proposal is syntactically unsound
 *  and will be invalid for all chain states.
 * @return true iff the proposal can be valid according to this oracle
 * @return false otherwise
 */
  bool isSound() override {
    coin_ = BinToUint64(Str2Bin(raw_data_), 0);
    amount_ = BinToInt64(Str2Bin(raw_data_), 8);
    addr_ = Str2Bin(raw_data_.substr(16));
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
    if (addr_.isNull()) {
      error_msg_ = "Recipient address is null";
      return false;
    }
    if (coin_ != 0) {
      error_msg_ = "Only allowed to request Devv.";
      return false;
    }
    if ((amount_ < 1) || (amount_ > 1000000)) {
      error_msg_ = "May only request positive amounts less than 1,000,000.";
      return false;
	}
	return true;
  }

/**
 *  @return if not valid or not sound, return an error message
 */
  std::string getErrorMessage() override {
    return(error_msg_);
  }

  //always empty for now
  std::map<uint64_t, std::vector<Tier2Transaction>>
      getTrace(const Blockchain& context) override {
    std::map<uint64_t, std::vector<Tier2Transaction>> out;
    return out;
  }

  uint64_t getCurrentDepth(const Blockchain& context) override {
    //@TODO(nick) scan pre-existing chain for this oracle instance.
    return(0);
  }

  std::map<uint64_t, std::vector<Tier2Transaction>>
      getNextTransactions(const Blockchain& context, const KeyRing& keys) override {
    std::map<uint64_t, std::vector<Tier2Transaction>> out;
    if (!isValid(context)) return out;
    std::vector<Transfer> xfers;
    Transfer pay(keys.getInnAddr(), coin_, -1*amount_, 0);
    Transfer settle(addr_, coin_, amount_, 0);
    xfers.push_back(pay);
    xfers.push_back(settle);
    std::vector<byte> nonce(Str2Bin(raw_data_));
    Tier2Transaction inn_tx(eOpType::Create, xfers, nonce,
                            keys.getKey(keys.getInnAddr()), keys);
    std::vector<Tier2Transaction> txs;
    txs.push_back(std::move(inn_tx));
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

/** Generate the appropriate signature(s) for this proposal.
 *
 * Oracle data to sign.
 */
  std::vector<byte> Sign() override {
    return Str2Bin(raw_data_);
  }

/**
 * @return the internal state of this oracle in JSON.
 */
  std::string getJSON() override {
    std::string json("{\"addr\":\""+addr_.getJSON()+"\",");
    json += "\"amount\":"+std::to_string(amount_)+",";
    json += "\"coin\":"+std::to_string(coin_)+"}";
    return json;
  }

private:
 std::string error_msg_;
 uint64_t coin_;
 Address addr_;
 int64_t amount_;

};

} // namespace Devv
