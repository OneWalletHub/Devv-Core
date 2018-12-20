/*
 * vote.h is an oracle to handle arbitrary election processes.
 *
 * @copywrite  2018 Devvio Inc
 *
 */
#pragma once

#include <string>

#include "oracleInterface.h"
#include "common/logger.h"
#include "primitives/Transaction.h"

//TODO: the election creates the vote tokens by giving them to the voters
namespace Devv {

class vote : public oracleInterface {

 public:
  vote(std::string data) : oracleInterface(data) {};

  Address election_;

  std::vector<Address> targets_;

  static Address getVoteSinkAddress() {
    std::vector<byte> bin(kNODE_ADDR_SIZE, 0);
    Address sink(bin);
    return sink;
  }

/**
 *  @return the string name that invokes this oracle
 */
  virtual std::string getOracleName() override {
    return (vote::GetOracleName());
  }

/**
 *  @return the string name that invokes this oracle
 */
  static std::string GetOracleName() {
    return ("io.devv.vote");
  }

  /**
 *  @return the shard used by this oracle
 */
  static uint64_t getShardIndex() {
    return (4);
  }

/**
 *  @return the coin type used by this oracle
 */
  static uint64_t getCoinIndex() {
    return (4);
  }

/** Checks if this proposal is objectively sound according to this oracle.
 *  When this function returns false, the proposal is syntactically unsound
 *  and will be invalid for all chain states.
 * @return true iff the proposal can be valid according to this oracle
 * @return false otherwise
 */
  bool isSound() override {
    //todo (nick) implement voting
    return false;
  }

/** Checks if this proposal is valid according to this oracle
 *  given a specific blockchain.
 * @params context the blockchain to check against
 * @return true iff the proposal is valid according to this oracle
 * @return false otherwise
 */
  bool isValid(const Blockchain& context) override {
    return isSound();
  }

/**
 *  @return if not valid or not sound, return an error message
 */
  std::string getErrorMessage() override {
    return ("WARNING: This oracle is a stub.");
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
    std::string json("{\"key\":\"" + ToHex(raw_data_) + "\"}");
    return json;
  }

  std::vector<byte> Sign() override {
    return getCanonical();
  }

 private:
  std::string error_msg_;
};

} // namespace Devv
