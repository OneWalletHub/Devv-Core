/*
 * json_interface.h
 * Tools to help dump the state of Devv primitives into JSON syntax.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "primitives/Summary.h"
#include "primitives/ProposedBlock.h"
#include "primitives/Validation.h"
#include "primitives/FinalBlock.h"
#include "consensus/blockchain.h"

namespace Devv {

/**
 *
 * @param input_summary
 * @return
 */
static std::string GetJSON(const Summary& input_summary) {
  std::string json("{\"" + kADDR_SIZE_TAG + "\":");
  auto summary_map = input_summary.getSummaryMap();
  uint64_t addr_size = summary_map.size();
  json += std::to_string(addr_size) + ",\"summary\":[";
  bool first_addr = true;
  for (auto summary : summary_map) {
    if (first_addr) {
      first_addr = false;
    } else {
      json += ",";
    }
    json += "{";
    json += "\"" + kADDR_TAG + "\":" + "\"" + summary.first.getJSON() + "\"" + ",";
    SummaryPair top_pair(summary.second);
    DelayedMap delayed(top_pair.first);
    CoinMap coin_map(top_pair.second);
    uint64_t delayed_size = delayed.size();
    uint64_t coin_size = coin_map.size();
    json += "\"" + kDELAY_SIZE_TAG + "\":" + std::to_string(delayed_size) + ",";
    json += "\"" + kCOIN_SIZE_TAG + "\":" + std::to_string(coin_size) + ",";
    bool is_first = true;
    json += "\"delayed\":[";
    for (auto delayed_item : delayed) {
      if (is_first) {
        is_first = false;
      } else {
        json += ",";
      }
      json += "{";
      json += "\"" + kTYPE_TAG + "\":" + std::to_string(delayed_item.first) + ",";
      json += "\"" + kDELAY_TAG + "\":" + std::to_string(delayed_item.second.delay) + ",";
      json += "\"" + kAMOUNT_TAG + "\":" + std::to_string(delayed_item.second.delta);
      json += "}";
    }
    json += "],\"coin_map\":[";
    is_first = true;
    for (auto coin : coin_map) {
      if (is_first) {
        is_first = false;
      } else {
        json += ",";
      }
      json += "{";
      json += "\"" + kTYPE_TAG + "\":" + std::to_string(coin.first) + ",";
      json += "\"" + kAMOUNT_TAG + "\":" + std::to_string(coin.second);
      json += "}";
    }
    json += "]}";
  }
  json += "]}";
  return json;
}

/**
 * Returns a JSON string representing this validation block.
 *
 * @param validation
 * @return a JSON string representing this validation block.
 */
static std::string GetJSON(const Validation& validation) {
  MTR_SCOPE_FUNC();
  std::string out("[");
  bool isFirst = true;
  for (auto const &item : validation.getValidationMap()) {
    if (isFirst) {
      isFirst = false;
    } else {
      out += ",";
    }
    out += "{";
    out += "\"" + item.first.getJSON() + "\":";
    out += "\"" + item.second.getJSON() + "\"";
    out += "}";
  }
  out += "]";
  return out;
}

/**
 * Returns a JSON representation of block as a string.
 *
 * @param block
 * @return a JSON representation of this block as a string.
 */
static std::string GetJSON(const ProposedBlock& block) {
  MTR_SCOPE_FUNC();
  std::string json("{\"" + kVERSION_TAG + "\":");
  json += std::to_string(block.getVersion()) + ",";
  json += "\"" + kBYTES_TAG + "\":" + std::to_string(block.getNumBytes()) + ",";
  std::vector<byte> prev_hash(std::begin(block.getPrevHash()), std::end(block.getPrevHash()));
  json += "\"" + kPREV_HASH_TAG + "\":\"" + ToHex(prev_hash) + "\",";
  json += "\"" + kTX_SIZE_TAG + "\":" + std::to_string(block.getSizeofTransactions()) + ",";
  json += "\"" + kSUM_SIZE_TAG + "\":" + std::to_string(block.getSummarySize()) + ",";
  json += "\"" + kVAL_COUNT_TAG + "\":" + std::to_string(block.getNumValidations()) + ",";
  json += "\"" + kTXS_TAG + "\":[";
  bool isFirst = true;
  for (auto const& item : block.getTransactions()) {
    if (isFirst) {
      isFirst = false;
    } else {
      json += ",";
    }
    json += item->getJSON();
  }
  json += "],\"" + kSUM_TAG + "\":" + GetJSON(block.getSummary()) + ",";
  json += "\"" + kVAL_TAG + "\":" + GetJSON(block.getValidation()) + "}";
  return json;
}

static std::string GetJSON(const FinalBlock& block) {
  std::string json("{\"" + kVERSION_TAG + "\":");
  json += std::to_string(block.getVersion()) + ",";
  json += "\"" + kBYTES_TAG + "\":" + std::to_string(block.getNumBytes()) + ",";
  json += "\"" + kTIME_TAG + "\":" + std::to_string(block.getBlockTime()) + ",";
  std::vector<byte> prev_hash(std::begin(block.getPreviousHash()), std::end(block.getPreviousHash()));
  json += "\"" + kPREV_HASH_TAG + "\":\"" + ToHex(prev_hash) + "\",";
  std::vector<byte> merkle(std::begin(block.getMerkleRoot()), std::end(block.getMerkleRoot()));
  json += "\"" + kMERKLE_TAG + "\":\"" + ToHex(merkle) + "\",";
  json += "\"" + kTX_SIZE_TAG + "\":" + std::to_string(block.getSizeofTransactions()) + ",";
  json += "\"" + kSUM_SIZE_TAG + "\":" + std::to_string(block.getSummarySize()) + ",";
  json += "\"" + kVAL_COUNT_TAG + "\":" + std::to_string(block.getNumValidations()) + ",";
  json += "\"" + kTXS_TAG + "\":[";
  bool isFirst = true;
  for (auto const& item : block.getTransactions()) {
    if (isFirst) {
      isFirst = false;
    } else {
      json += ",";
    }
    json += item->getJSON();
  }
  json += "],\"" + kSUM_TAG + "\":" + GetJSON(block.getSummary()) + ",";
  json += "\"" + kVAL_TAG + "\":" + GetJSON(block.getValidation()) + "}";
  return json;
}

static std::string GetJSON(const Blockchain& blockchain) {
  std::string out("[");
  bool first = true;
  for (auto const &item : blockchain.getBlockVector()) {
    if (first) {
      first = false;
    } else {
      out += ",";
    }
    out += GetJSON(*item);
  }
  out += "]";
  return out;
}

}
