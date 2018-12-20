/*
 * Tier2Transaction.cpp
 * implements methods to generate and copy Tier2Transactions.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "primitives/Tier2Transaction.h"
#include "common/devv_exceptions.h"

namespace Devv {

void Tier2Transaction::QuickFill(Tier2Transaction& tx,
                    InputBuffer &buffer) {
  MTR_SCOPE_FUNC();
  int trace_int = 124;
  MTR_START("Transaction", "Transaction", &trace_int);
  MTR_STEP("Transaction", "Transaction", &trace_int, "step1");

  if (buffer.size() < buffer.getOffset() + MinSize()) {
    throw DeserializationError("Invalid serialized T2 transaction, too small!");
  }
  /// Don't increment the buffer, we want to copy it all to canonical_
  tx.xfer_size_ = buffer.getNextUint64(false);
  tx.nonce_size_ = buffer.getSecondUint64(false);

  if (tx.nonce_size_ < minNonceSize()) {
    std::stringstream ss;
    std::vector<byte> prefix(buffer.getCurrentIterator()
        , buffer.getCurrentIterator() + kUINT64_SIZE);
    ss << "Invalid serialized T2 transaction, bad nonce size ("
        + std::to_string(tx.nonce_size_) + ")!";
    ss << "Transaction prefix: " + ToHex(prefix);
    ss << "Bytes offset: " + std::to_string(buffer.getOffset());
    throw DeserializationError(ss.str());
  }

  byte oper = buffer.offsetAt(kOPERATION_OFFSET);
  if (oper >= eOpType::NumOperations) {
    std::stringstream ss;
    ss << "Invalid serialized T2 transaction, invalid operation!";
    throw DeserializationError(ss.str());
  }

  size_t tx_size = buffer.offsetAt(kTRANSFER_OFFSET+tx.xfer_size_+tx.nonce_size_)
                   +kTRANSFER_OFFSET+tx.xfer_size_+tx.nonce_size_+1;

  if (buffer.size() < buffer.getOffset() + tx_size) {
    std::stringstream ss;
    std::vector<byte> prefix(buffer.getCurrentIterator()
        , buffer.getCurrentIterator() + 8);
    ss << "Invalid serialized T2 transaction, wrong size ("
        + std::to_string(tx_size) + ")!";
    ss << "Transaction prefix: " + ToHex(prefix);
    ss << "Bytes offset: " + std::to_string(buffer.getOffset());
    throw DeserializationError(ss.str());
  }
  MTR_STEP("Transaction", "Transaction", &trace_int, "step2");
  buffer.copy(std::back_inserter(tx.canonical_), tx_size);
}

void Tier2Transaction::Fill(Tier2Transaction& tx,
                    InputBuffer &buffer,
                    const KeyRing &keys,
                    bool calculate_soundness) {
  MTR_SCOPE_FUNC();
  int trace_int = 124;
  MTR_START("Transaction", "Transaction", &trace_int);
  MTR_STEP("Transaction", "Transaction", &trace_int, "step1");

  if (buffer.size() < buffer.getOffset() + MinSize()) {
    throw DeserializationError("Invalid serialized T2 transaction, too small!");
  }
  /// Don't increment the buffer, we want to copy it all to canonical_
  tx.xfer_size_ = buffer.getNextUint64(false);
  tx.nonce_size_ = buffer.getSecondUint64(false);

  if (tx.nonce_size_ < minNonceSize()) {
    std::stringstream ss;
    std::vector<byte> prefix(buffer.getCurrentIterator()
        , buffer.getCurrentIterator() + 8);
    ss << "Invalid serialized T2 transaction, bad nonce size ("
        + std::to_string(tx.nonce_size_) + ")!";
    ss << "Transaction prefix: " + ToHex(prefix);
    ss << "Bytes offset: " + std::to_string(buffer.getOffset());
    throw DeserializationError(ss.str());
  }

  byte oper = buffer.offsetAt(16);
  if (oper >= eOpType::NumOperations) {
    std::stringstream ss;
    ss << "Invalid serialized T2 transaction, invalid operation!";
    throw DeserializationError(ss.str());
  }

  size_t tx_size = buffer.offsetAt(kTRANSFER_OFFSET+tx.xfer_size_+tx.nonce_size_)
                   +kTRANSFER_OFFSET+tx.xfer_size_+tx.nonce_size_+1;

  if (buffer.size() < buffer.getOffset() + tx_size) {
    std::stringstream ss;
    std::vector<byte> prefix(buffer.getCurrentIterator()
        , buffer.getCurrentIterator() + 8);
    ss << "Invalid serialized T2 transaction, wrong size ("
        + std::to_string(tx_size) + ")!";
    ss << "Transaction prefix: " + ToHex(prefix);
    ss << "Bytes offset: " + std::to_string(buffer.getOffset());
    throw DeserializationError(ss.str());
  }
  buffer.copy(std::back_inserter(tx.canonical_), tx_size);

  MTR_STEP("Transaction", "Transaction", &trace_int, "sound");

  if (calculate_soundness) {
    tx.is_sound_ = tx.isSound(keys);
    if (!tx.is_sound_) {
      LOG_WARNING << "Invalid serialized T2 transaction, not sound!";
    }
  }
  MTR_FINISH("Transaction", "Transaction", &trace_int);
}

Tier2Transaction Tier2Transaction::Create(InputBuffer& buffer,
                                          const KeyRing& keys,
                                          bool calculate_soundness) {
  Tier2Transaction tx;
  Tier2Transaction::Fill(tx, buffer, keys, calculate_soundness);
  return tx;
}

Tier2Transaction Tier2Transaction::QuickCreate(InputBuffer& buffer) {
  Tier2Transaction tx;
  Tier2Transaction::QuickFill(tx, buffer);
  return tx;
}

Tier2TransactionPtr Tier2Transaction::CreateUniquePtr(InputBuffer& buffer,
                                                      const KeyRing& keys,
                                                      bool calculate_soundness)
{
  auto tx = std::make_unique<Tier2Transaction>();
  Tier2Transaction::Fill(*tx, buffer, keys, calculate_soundness);
  return tx;
}

} // namespace Devv
