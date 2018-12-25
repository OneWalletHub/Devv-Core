/*
 * devv_pbuf.h tools for integration with Protobuf formatted data.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <vector>
#include <exception>
#include <typeinfo>

#include "primitives/blockchain.h"
#include "primitives/Tier2Transaction.h"
#include "oracles/api.h"
#include "oracles/coin_request.h"
#include "oracles/data.h"
#include "oracles/dcash.h"
#include "oracles/do_transaction.h"
#include "oracles/devvprotect.h"
#include "oracles/dneroavailable.h"
#include "oracles/dnerowallet.h"
#include "oracles/id.h"
#include "oracles/revert.h"
#include "oracles/vote.h"

#include "devv.pb.h"

namespace Devv {

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

struct PendingTransaction {
  Signature sig;
  uint32_t expect_block = 0;
  uint32_t shard_index = 0;
};
typedef std::unique_ptr<PendingTransaction> PendingTransactionPtr;

struct AnnouncerResponse {
  uint32_t return_code = 0;
  std::string message;
  std::vector<PendingTransactionPtr> pending;
};
typedef std::unique_ptr<AnnouncerResponse> AnnouncerResponsePtr;

struct RepeaterRequest {
  int64_t timestamp = 0;
  uint32_t operation = 0;
  std::string uri;
};
typedef std::unique_ptr<RepeaterRequest> RepeaterRequestPtr;

struct RepeaterResponse {
  int64_t request_timestamp = 0;
  uint32_t operation = 0;
  uint32_t return_code = 0;
  std::string message;
  std::vector<byte> raw_response;
};
typedef std::unique_ptr<RepeaterResponse> RepeaterResponsePtr;

struct ServiceRequest {
  int64_t timestamp = 0;
  std::string endpoint;
  std::map<std::string, std::string> args;
};
typedef std::unique_ptr<ServiceRequest> ServiceRequestPtr;

struct ServiceResponse {
  int64_t request_timestamp = 0;
  std::string endpoint;
  uint32_t return_code = 0;
  std::string message;
  std::map<std::string, std::string> args;
};
typedef std::unique_ptr<ServiceResponse> ServiceResponsePtr;

TransactionPtr CreateTransaction(const devv::proto::Transaction& transaction,
                                 const KeyRing& keys,
                                 bool do_sign = false);

Tier2TransactionPtr CreateTransaction(const devv::proto::Transaction& transaction, std::string pk, std::string pk_pass);

std::vector<TransactionPtr> ValidateOracle(oracleInterface& oracle, const Blockchain& chain, const KeyRing& keys);

std::vector<TransactionPtr> DecomposeProposal(const devv::proto::Proposal& proposal,
                                              const Blockchain& chain,
                                              const KeyRing& keys);

std::vector<TransactionPtr> DeserializeEnvelopeProtobufString(
                                              const std::string& pb_envelope,
                                              const KeyRing& keys,
                                              const Blockchain& context);

TransactionPtr DeserializeTxProtobufString(const std::string& pb_tx, const KeyRing& keys, bool do_sign = false);

devv::proto::AnnouncerResponse SerializeAnnouncerResponse(const AnnouncerResponsePtr& response_ptr);

RepeaterRequestPtr DeserializeRepeaterRequest(const std::string& pb_request);

devv::proto::RepeaterResponse SerializeRepeaterResponse(const RepeaterResponsePtr& response_ptr);

ServiceRequestPtr DeserializeServiceRequest(const std::string& pb_request);

devv::proto::ServiceResponse SerializeServiceResponse(const ServiceResponsePtr& response_ptr);

devv::proto::Transaction SerializeTransaction(const Tier2Transaction& one_tx, devv::proto::Transaction& tx);

devv::proto::FinalBlock SerializeFinalBlock(const FinalBlock& block);

devv::proto::Envelope SerializeEnvelopeFromBinaryTransactions(const std::vector<std::vector<byte>>& txs);

} // namespace Devv
