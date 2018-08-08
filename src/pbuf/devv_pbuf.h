/*
 * ${Filename}
 *
 *  Created on: 8/6/18
 *      Author: mckenney
 */
#ifndef DEVCASH_DEVV_PBUF_H
#define DEVCASH_DEVV_PBUF_H

#include <vector>
#include <exception>

#include "primitives/Tier2Transaction.h"

#include "devv.pb.h"

namespace Devcash {

struct Proposal {
  std::string oracle_name;
  std::string data;
};
typedef std::unique_ptr<Proposal> ProposalPtr;

struct Envelope {
  std::vector<Tier2TransactionPtr> transactions;
  std::vector<ProposalPtr> proposals;
};
typedef std::unique_ptr<Envelope> EnvelopePtr;

Tier2TransactionPtr CreateTransaction(Devv::proto::Transaction& transaction, const KeyRing& keys) {
  auto operation = transaction.operation();
  auto pb_xfers = transaction.xfers();

  std::vector<Transfer> transfers;
  EC_KEY* key = nullptr;
  for (auto const& xfer : pb_xfers) {
    std::vector<byte> bytes(xfer.address().begin(), xfer.address().end());
    auto address = Address(bytes);
    transfers.emplace_back(address, xfer.coin(), xfer.amount(), xfer.delay());
    if (xfer.amount() < 0) {
      if (key != nullptr) {
        throw std::runtime_error("More than one send transfer not supported.");
      }
      key = keys.getKey(address);
    }
  }

  if (key == nullptr) {
    throw std::runtime_error("EC_KEY from sender not set (key == null)");
  }

  std::vector<byte> nonce(transaction.nonce().begin(), transaction.nonce().end());
  std::vector<byte> sig(transaction.sig().begin(), transaction.sig().end());
  Signature signature(sig);

  Tier2TransactionPtr t2tx_ptr = std::make_unique<Tier2Transaction>(
      operation,
      transfers,
      nonce,
      key,
      keys,
      signature);

  return t2tx_ptr;
}

EnvelopePtr DeserializeEnvelopeProtobufString(const std::string& pb_envelope, const KeyRing& keys) {
  Devv::proto::Envelope envelope;
  envelope.ParseFromString(pb_envelope);

  EnvelopePtr env_ptr = std::make_unique<Envelope>();
  return env_ptr;
}

Tier2TransactionPtr GetT2TxFromProtobufString(const std::string& pb_tx, const KeyRing& keys) {

  Devv::proto::Transaction tx;
  tx.ParseFromString(pb_tx);

  auto t2tx_ptr = CreateTransaction(tx, keys);

  return(t2tx_ptr);
}

} // namespace Devcash

#endif //DEVCASH_DEVV_PBUF_H
