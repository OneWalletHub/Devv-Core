/*
 * FinalBlock.h
 * Defines the structure of a Devv final block.
 *
 * @copywrite  2018 Devvio Inc
 */
#ifndef PRIMITIVES_FINALBLOCK_H_
#define PRIMITIVES_FINALBLOCK_H_

#include "common/devv_exceptions.h"
#include "primitives/ProposedBlock.h"

namespace Devv {

/**
 * Contains a finalized blockchain block
 */
class FinalBlock {
 public:
  /**
   * Constructor
   * @param proposed
   */
  explicit FinalBlock(const ProposedBlock& proposed)
      : num_bytes_(proposed.getNumBytes() + 40),
        block_time_(GetMillisecondsSinceEpoch()),
        prev_hash_(proposed.getPrevHash()),
        merkle_root_(),
        tx_size_(proposed.getSizeofTransactions()),
        sum_size_(proposed.getSummarySize()),
        val_count_(proposed.getNumValidations()),
        transaction_vector_(Copy(proposed.getTransactions())),
        raw_transactions_(proposed.getRawTransactions()),
        summary_(Summary::Copy(proposed.getSummary())),
        vals_(proposed.getValidation()),
        block_state_(proposed.getBlockState()) {
    merkle_root_ = DevvHash(getBlockDigest());
    std::vector<byte> merkle(std::begin(merkle_root_), std::end(merkle_root_));
    LOG_INFO << "Merkle: " + ToHex(merkle);
  }

  /**
   * Constructor
   * @param serial
   * @param prior
   * @param keys
   * @param tcm
   */
  FinalBlock(InputBuffer& buffer,
             const ChainState& prior,
             const KeyRing& keys,
             TransactionCreationManager& tcm) : block_state_(prior) {
    if (buffer.size() < MinSize()) {
      LOG_WARNING << "Invalid serialized FinalBlock, too small!";
      return;
    }
    version_ |= buffer.getNextByte();
    if (version_ != 0) {
      LOG_WARNING << "Invalid FinalBlock.version: " + std::to_string(version_);
      return;
    }
    num_bytes_ = buffer.getNextUint64();
    if (buffer.size() != num_bytes_) {
      LOG_WARNING << "Invalid serialized FinalBlock, wrong size!";
      return;
    }
    block_time_ = buffer.getNextUint64();
    buffer.copy(prev_hash_);
    buffer.copy(merkle_root_);
    tx_size_ = buffer.getNextUint64();
    sum_size_ = buffer.getNextUint64();
    val_count_ = buffer.getNextUint32();

    tcm.set_keys(&keys);
    raw_transactions_ = tcm.CreateTransactions(buffer, transaction_vector_, MinSize(), tx_size_);

    for (auto& tx : transaction_vector_) {
      auto summary = Summary::Create();
      tx->isValid(block_state_, keys, summary);
      LOG_DEBUG << "FINALBLOCK VERIFIED blk("<<block_state_.size()<<"): " << tx->getSignature().getJSON();
    }

    summary_ = Summary::Create(buffer);
    vals_ = Validation::Create(buffer);
  }

  /**
   * Constructor
   * @param prior
   * @param keys
   * @param mode
   */
  FinalBlock(InputBuffer& buffer,
             const ChainState& prior,
             const KeyRing& keys,
             eAppMode mode)
      : block_state_(prior) {
    if (buffer.size() < MinSize()) {
      throw std::runtime_error("Invalid serialized FinalBlock, too small!");
    }
    version_ |= buffer.getNextByte();
    if (version_ != 0) {
      throw std::runtime_error("Invalid FinalBlock.version: " + std::to_string(version_));
    }

    num_bytes_ = buffer.getNextUint64();
    if (buffer.size() < num_bytes_) {
      throw std::runtime_error("Invalid serialized FinalBlock, wrong size!");
    }
    block_time_ = buffer.getNextUint64();
    buffer.copy(prev_hash_);
    buffer.copy(merkle_root_);
    tx_size_ = buffer.getNextUint64();
    sum_size_ = buffer.getNextUint64();
    val_count_ = buffer.getNextUint32();

    size_t tx_start = buffer.getOffset();
    while (buffer.getOffset() < tx_start + tx_size_) {
      if (mode == eAppMode::T1) {
        Tier1TransactionPtr one_tx = std::make_unique<Tier1Transaction>(buffer, keys);
        raw_transactions_.push_back(one_tx->getCanonical());
        transaction_vector_.push_back(std::move(one_tx));
      } else if (mode == eAppMode::T2) {
        Tier2TransactionPtr one_tx = Tier2Transaction::CreateUniquePtr(buffer, keys);
        raw_transactions_.push_back(one_tx->getCanonical());
        transaction_vector_.push_back(std::move(one_tx));
        for (auto& tx : transaction_vector_) {
          auto summary = Summary::Create();
          tx->isValid(block_state_, keys, summary);
        }
      } else {
        throw std::runtime_error("Unsupported mode: "+std::to_string(mode));
      }
    }

    summary_ = Summary::Create(buffer);
    vals_ = Validation::Create(buffer);
  }

  /**
   * Constructor
   * @param other
   */
  FinalBlock(const FinalBlock& other)
      : version_(other.version_)
      , num_bytes_(other.num_bytes_)
      , block_time_(other.block_time_)
      , prev_hash_(other.prev_hash_)
      , merkle_root_(other.merkle_root_)
      , tx_size_(other.tx_size_)
      , sum_size_(other.sum_size_)
      , val_count_(other.val_count_)
      , transaction_vector_(Copy(other.transaction_vector_))
      , raw_transactions_(other.raw_transactions_)
      , summary_(Summary::Copy(other.summary_))
      , vals_(other.vals_)
      , block_state_(other.block_state_){}

  /**
   *
   * @param buffer
   * @param prior
   * @return
   */
  static FinalBlock Create(InputBuffer& buffer, const ChainState& prior);

  /**
   * Static method which returns the minimum size for a FinalBlock
   * @return minimum size (101)
   */
  static size_t MinSize() {
    /// @todo (mckenney) define hard-coded value
    return 101;
  }

  /**
   *
   * @return
   */
  const std::vector<TransactionPtr>& getTransactions() const { return transaction_vector_; }

  const std::vector<std::vector<byte>>& getRawTransactions() const {   return raw_transactions_;  }

  /**
   * Returns the number of transactions in this block
   * @return number of transactions
   */
  size_t getNumTransactions() const { return raw_transactions_.size(); }

  /**
   * Returns the number of validations
   * @return the number of validations
   */
  uint32_t getNumValidations() const { return val_count_; }

  /**
   * Returns copies of the transactions recorded in this block.
   * @return a vector of pointers to transactions
   */
  std::vector<TransactionPtr> CopyTransactions() const {
    std::vector<TransactionPtr> out;
    for (const auto& e : transaction_vector_) {
      out.push_back(e->clone());
    }
    return out;
  }

  /**
   * Returns the number of coin transfers in this block.
   * @return number of transfers
   */
  size_t getNumTransfers() const {
    size_t tfers = 0;
    for (auto const& item : transaction_vector_) {
      tfers += item->getTransfers().size();
    }
    return tfers;
  }

  /**
   * Return the block version
   * @return block version
   */
  uint8_t getVersion() const { return version_; }

  /**
   * Returns block digest as a byte vector
   * @return
   */
  std::vector<byte> getBlockDigest() const {
    std::vector<byte> txs;
    for (auto const& item : raw_transactions_) {
      txs.insert(txs.end(), item.begin(), item.end());
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

  /**
   * Returns a CBOR representation of this block as a byte vector.
   * @return a CBOR representation of this block as a byte vector.
   */
  std::vector<byte> getCanonical() const {
    std::vector<byte> txs;
    for (auto const& item : raw_transactions_) {
      txs.insert(txs.end(), item.begin(), item.end());
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

  /**
   * Return the number of bytes
   * @return number of bytes
   */
  uint64_t getNumBytes() const { return num_bytes_; }

  /**
   * Returns the size of the transactions
   * @return the size of the transactions
   */
  uint64_t getSizeofTransactions() const { return tx_size_; }

  /**
   * Return the time of this block
   * @return block time in milliseconds since the Epoch
   */
  uint64_t getBlockTime() const { return block_time_; }

  /**
   * Return a const ref to the previous hash
   * @return
   */
  const Hash& getPreviousHash() const { return prev_hash_; }

  /**
   * Return a const ref to the merkle root
   * @return
   */
  const Hash& getMerkleRoot() const { return merkle_root_; }

  /**
   * Return a const ref to the ChainState
   * @return
   */
  const ChainState& getChainState() const { return block_state_; }

  /**
   * Return a const ref to the Summary
   * @return
   */
  const Summary& getSummary() const { return summary_; }

  /**
   * Returns the size of the Summary
   * @return the size of the Summary
   */
  uint64_t getSummarySize() const { return sum_size_; }

  /**
   * Return a const ref to the validation
   * @return
   */
  const Validation& getValidation() const { return vals_; }

  /**
   * Return a ref to the validation
   * @return
   */
  Validation& getValidation() { return vals_; }

  /**
   * @return the volume of coins moved in this block.
   */
  uint64_t getVolume() const {
    uint64_t volume = 0;
    for (auto const& item : transaction_vector_) {
      for (auto const& xfer : item->getTransfers()) {
        volume += llabs(xfer->getAmount());
      }
    }
    return volume;
  }

  /**
   * @return the summed value of coins moved in this block.
   */
  int64_t getValue() const {
    int64_t value = 0;
    for (auto const& item : transaction_vector_) {
      for (auto const& xfer : item->getTransfers()) {
        value += xfer->getAmount();
      }
    }
    return value;
  }

 private:
  /**
   * Constructor
   * @param prior
   */
  explicit FinalBlock(const ChainState& prior) : block_state_(prior) {}

  /// Version of the block
  uint8_t version_ = 0;
  /// Number of bytes of this block
  uint64_t num_bytes_ = 0;
  /// Number of milliseconds since the Epoch
  uint64_t block_time_ = 0;
  /// The has of the previous block
  Hash prev_hash_ = {};
  /// The merkle root
  Hash merkle_root_ = {};
  /// Size of Transactions in this block
  uint64_t tx_size_ = 0;
  /// Size of the Summary
  uint64_t sum_size_ = 0;
  /// Number of Validations
  uint32_t val_count_ = 0;
  /// vector of TransactionPtrs
  std::vector<TransactionPtr> transaction_vector_;
  /// vector of canonical transactions
  std::vector<std::vector<byte>> raw_transactions_;
  /// Summary
  Summary summary_ = Summary::Create();
  /// Validation
  Validation vals_ = Validation::Create();
  /// ChainState
  ChainState block_state_;
};

typedef std::shared_ptr<FinalBlock> FinalBlockSharedPtr;
typedef std::shared_ptr<const FinalBlock> ConstFinalBlockSharedPtr;

}  // end namespace Devv

#endif /* PRIMITIVES_FINALBLOCK_H_ */
