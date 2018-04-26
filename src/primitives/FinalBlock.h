/*
 * FinalBlock.h
 *
 *  Created on: Apr 21, 2018
 *      Author: Nick Williams
 */

#ifndef PRIMITIVES_FINALBLOCK_H_
#define PRIMITIVES_FINALBLOCK_H_

#include "ProposedBlock.h"

using namespace Devcash;

namespace Devcash
{

class FinalBlock {

public:

  /** Constructors */
  //FinalBlock() = 0;
  FinalBlock(const ProposedBlock& proposed)
    : num_bytes_(proposed.num_bytes_+40), block_time_(getEpoch())
    , prev_hash_(proposed.prev_hash_), merkle_root_()
    , tx_size_(proposed.tx_size_), sum_size_(proposed.sum_size_)
    , val_count_(proposed.val_count_), vtx_(proposed.getTransactions())
    , summary_(proposed.getSummary()), vals_(proposed.getValidation())
    , block_state_(proposed.getBlockState())
  {
    merkle_root_ = dcHash(getBlockDigest());
    std::vector<byte> merkle(std::begin(merkle_root_), std::end(merkle_root_));
    LOG_INFO << "Merkle: "+toHex(merkle);
  }
  FinalBlock(const std::vector<byte>& serial, const ChainState& prior
      ,const KeyRing& keys) : num_bytes_(0), block_time_(0), prev_hash_()
      , merkle_root_(), tx_size_(0), sum_size_(0), val_count_(0), vtx_()
      , summary_(), vals_(), block_state_(prior) {
    if (serial.size() < MinSize()) {
      LOG_WARNING << "Invalid serialized FinalBlock, too small!";
      return;
    }
    version_ |= serial.at(0);
    if (version_ != 0) {
      LOG_WARNING << "Invalid FinalBlock.version: "+std::to_string(version_);
      return;
    }
    size_t offset = 1;
    num_bytes_ = BinToUint64(serial, offset);
    offset += 8;
    if (serial.size() != num_bytes_) {
      LOG_WARNING << "Invalid serialized FinalBlock, wrong size!";
      return;
    }
    std::vector<byte> sub(std::begin(serial)+offset, std::begin(serial)+offset+8);
    block_time_ = BinToUint64(serial, offset);
    offset += 8;
    std::copy_n(serial.begin()+offset, SHA256_DIGEST_LENGTH, prev_hash_.begin());
    offset += 32;
    std::copy_n(serial.begin()+offset, SHA256_DIGEST_LENGTH, merkle_root_.begin());
    offset += 32;
    tx_size_ = BinToUint64(serial, offset);
    offset += 8;
    sum_size_ = BinToUint64(serial, offset);
    offset += 8;
    val_count_ = BinToUint32(serial, offset);
    offset += 4;
    while (offset < MinSize()+tx_size_) {
      //Transaction constructor increments offset by ref
      Transaction one_tx(serial, offset, keys);
      vtx_.push_back(one_tx);
    }
    Summary temp(serial, offset);
    summary_ = temp;
    Validation val_temp(serial, offset);
    vals_ = val_temp;
  }
  FinalBlock(const FinalBlock& other)
      : version_(other.version_)
      , num_bytes_(other.num_bytes_), block_time_(other.block_time_)
      , prev_hash_(other.prev_hash_), merkle_root_(other.merkle_root_)
      , tx_size_(other.tx_size_), sum_size_(other.sum_size_)
      , val_count_(other.val_count_), vtx_(other.vtx_), summary_(other.summary_)
      , vals_(other.vals_), block_state_(other.block_state_){}

  static size_t MinSize() {
    return 101;
  }

/** Returns a JSON representation of this block as a string.
 *  @return a JSON representation of this block as a string.
*/
  std::string getJSON() const {
    std::string json("{\""+kVERSION_TAG+"\":");
    json += std::to_string(version_)+",";
    json += "\""+kBYTES_TAG+"\":"+std::to_string(num_bytes_)+",";
    json += "\""+kTIME_TAG+"\":"+std::to_string(block_time_)+",";
    std::vector<byte> prev_hash(std::begin(prev_hash_), std::end(prev_hash_));
    json += "\""+kPREV_HASH_TAG+"\":"+toHex(prev_hash)+",";
    std::vector<byte> merkle(std::begin(merkle_root_), std::end(merkle_root_));
    json += "\""+kMERKLE_TAG+"\":"+toHex(merkle)+",";
    json += "\""+kTX_SIZE_TAG+"\":"+std::to_string(tx_size_)+",";
    json += "\""+kSUM_SIZE_TAG+"\":"+std::to_string(sum_size_)+",";
    json += "\""+kVAL_COUNT_TAG+"\":"+std::to_string(val_count_)+",";
    json += "\""+kTXS_TAG+"\":[";
    bool isFirst = true;
    for (auto const& item : vtx_) {
      if (isFirst) {
        isFirst = false;
      } else {
        json += ",";
      }
      json += item.getJSON();
    }
    json += "],\""+kSUM_TAG+"\":"+summary_.getJSON()+",";
    json += "\""+kVAL_TAG+"\":"+vals_.getJSON()+"}";
    return json;
  }

  std::vector<byte> getBlockDigest() const {
    std::vector<byte> txs;
    for (auto const& item : vtx_) {
      const std::vector<byte> txs_canon(item.getCanonical());
      txs.insert(txs.end(), txs_canon.begin(), txs_canon.end());
    }
    const std::vector<byte> sum_canon(summary_.getCanonical());
    const std::vector<byte> val_canon(vals_.getCanonical());

    std::vector<byte> serial;
    serial.reserve(num_bytes_);
    serial.push_back(version_ & 0xFF);
    Uint64ToBin(num_bytes_, serial);
    Uint64ToBin(block_time_, serial);
    serial.insert(serial.end(), prev_hash_.begin(), prev_hash_.end());
    Uint64ToBin(tx_size_, serial);
    Uint64ToBin(sum_size_, serial);
    Uint32ToBin(val_count_, serial);
    serial.insert(serial.end(), txs.begin(), txs.end());
    serial.insert(serial.end(), sum_canon.begin(), sum_canon.end());
    serial.insert(serial.end(), val_canon.begin(), val_canon.end());

    return serial;
  }

/** Returns a CBOR representation of this block as a byte vector.
 *  @return a CBOR representation of this block as a byte vector.
*/
  std::vector<byte> getCanonical() const {
    std::vector<byte> txs;
    for (auto const& item : vtx_) {
      const std::vector<byte> txs_canon(item.getCanonical());
      txs.insert(txs.end(), txs_canon.begin(), txs_canon.end());
    }
    const std::vector<byte> sum_canon(summary_.getCanonical());
    const std::vector<byte> val_canon(vals_.getCanonical());

    std::vector<byte> serial;
    serial.reserve(num_bytes_);
    serial.push_back(version_ & 0xFF);
    Uint64ToBin(num_bytes_, serial);
    Uint64ToBin(block_time_, serial);
    serial.insert(serial.end(), prev_hash_.begin(), prev_hash_.end());
    serial.insert(serial.end(), merkle_root_.begin(), merkle_root_.end());
    Uint64ToBin(tx_size_, serial);
    Uint64ToBin(sum_size_, serial);
    Uint32ToBin(val_count_, serial);
    serial.insert(serial.end(), txs.begin(), txs.end());
    serial.insert(serial.end(), sum_canon.begin(), sum_canon.end());
    serial.insert(serial.end(), val_canon.begin(), val_canon.end());

    return serial;
  }

  Hash getMerkleRoot() const {
    return merkle_root_;
  }

  ChainState getChainState() const {
    return block_state_;
  }

private:
  uint8_t version_ = 0;
  uint64_t num_bytes_;
  uint64_t block_time_;
  Hash prev_hash_;
  Hash merkle_root_;
  uint64_t tx_size_;
  uint64_t sum_size_;
  uint32_t val_count_;
  std::vector<Transaction> vtx_;
  Summary summary_;
  Validation vals_;
  ChainState block_state_;

};

typedef std::shared_ptr<FinalBlock> FinalPtr;

} //end namespace Devcash


#endif /* PRIMITIVES_FINALBLOCK_H_ */
