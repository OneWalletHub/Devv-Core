/*
 * revert.h is an oracle to reverse transactions.
 *
 * @copywrite  2018 Devvio Inc
 *
 */
#pragma once

#include <string>

#include "dneroavailable.h"
#include "dnerowallet.h"
#include "oracleInterface.h"
#include "common/logger.h"
#include "consensus/chainstate.h"

namespace Devv {

class revert : public oracleInterface {

 public:

  revert(std::string data) : oracleInterface(data) {};

  const long kDEFAULT_DELAY = 604800; //1 week in seconds

/**
 *  @return the string name that invokes this oracle
 */
  virtual std::string getOracleName() override {
    return (revert::GetOracleName());
  }

/**
 *  @return the string name that invokes this oracle
 */
  static std::string GetOracleName() {
    return ("revert");
  }

  /**
 *  @return the shard used by this oracle
 */
  static uint64_t getShardIndex() {
    return (1);
  }

/**
 *  @return the coin type used by this oracle
 */
  static uint64_t getCoinIndex() {
    return (0);
  }

/** Checks if this proposal is objectively sound according to this oracle.
 *  When this function returns false, the proposal is syntactically unsound
 *  and will be invalid for all chain states.
 * @return true iff the proposal can be valid according to this oracle
 * @return false otherwise
 */
  bool isSound() override {
    Signature sig(Str2Bin(raw_data_));
    if (sig.isNull()) {
      error_msg_ = "Unsound signature.";
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
    std::vector<byte> raw(Str2Bin(raw_data_));
    std::map<std::vector<byte>, std::vector<byte>> hits = context.TraceTransactions(raw);
    if (hits.empty()) return false;
    for (auto the_pair : hits) {
      if (the_pair.first == raw) return true;
    }
    return false;
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
    return 1;
  }

  uint64_t getMaxDepth() override {
    return 1;
  }

  std::map<uint64_t, std::vector<Tier2Transaction>>
  getNextTransactions(const Blockchain& context, const KeyRing& keys) override {
    std::map<uint64_t, std::vector<Tier2Transaction>> out;
    if (!isSound()) return out;
    std::vector<Tier2Transaction> txs;
    std::vector<byte> raw(Str2Bin(raw_data_));
    std::map<std::vector<byte>, std::vector<byte>> hits = context.TraceTransactions(raw);
    if (hits.empty()) return out;
    for (auto the_pair : hits) {
      if (the_pair.first == raw) {
        InputBuffer buffer(the_pair.second);
        Tier2Transaction to_revert = Tier2Transaction::QuickCreate(buffer);

        //construct revert matching transaction
        std::vector<Transfer> xfers;
        for (auto const& xfer : to_revert.getTransfers()) {
          if (xfer->getDelay() > 0) {
            Transfer inverse(xfer->getAddress(), xfer->getCoin(), -1*xfer->getAmount(), 0);
            xfers.push_back(inverse);
          }
        }
        Tier2Transaction revert_tx(eOpType::Revert, xfers, raw,
              keys.getKey(keys.getInnAddr()), keys);
        txs.push_back(std::move(revert_tx));
	  }
    }
    std::pair<uint64_t, std::vector<Tier2Transaction>> p(getShardIndex(), std::move(txs));
    out.insert(std::move(p));
    return out;
  }

/** Recursively generate the state of this oracle and all dependent oracles.
 *
 * @pre This transaction must be valid.
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
 * @pre This transaction must be valid.
 * @params context the blockchain of the shard that provides context for this oracle
 * @return a map of oracles to data encoded in JSON
 */
  virtual std::map<std::string, std::string>
  getDecompositionMapJSON(const Blockchain& context) override {
    std::map<std::string, std::string> out;
    std::pair<std::string, std::string> p(getOracleName(), getJSON());
    out.insert(p);
    return out;
  }

/**
 * @return the internal state of this oracle in JSON.
 */
  std::string getJSON() override {
    Signature sig(Str2Bin(raw_data_));
    return sig.getJSON();
  }

  std::vector<byte> getProposal() override {
    return getCanonical();
  }

  Signature getRootSignature() override {
    Signature sig(Str2Bin(raw_data_));
    return sig;
  }

  std::vector<byte> getInitialState() override {
    return getCanonical();
  }

 private:
  std::string error_msg_;

};

} // namespace Devv
