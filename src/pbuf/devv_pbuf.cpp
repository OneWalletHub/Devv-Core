/*
 * devv_pbuf.cpp tools for integration with Protobuf formatted data.
 *
 * @copywrite  2018 Devvio Inc
 */
#include "pbuf/devv_pbuf.h"
#include "io/blockchain_request_handlers.h"

namespace Devv {

TransactionPtr CreateTransaction(const devv::proto::Transaction& transaction, const KeyRing& keys, bool do_sign) {
  auto operation = transaction.operation();
  auto pb_xfers = transaction.xfers();

  std::vector<Transfer> transfers;
  EC_KEY* key = nullptr;
  for (auto const& xfer : pb_xfers) {
    std::vector<byte> bytes(xfer.address().begin(), xfer.address().end());
    auto address = Address(bytes);
    LOG_INFO << "Transfer addr: "+ToHex(xfer.address());
    LOG_INFO << "Coin: "+std::to_string(xfer.coin());
    LOG_INFO << "Amount: "+std::to_string(xfer.amount());
    LOG_INFO << "Delay: "+std::to_string(xfer.delay());
    transfers.emplace_back(address, xfer.coin(), xfer.amount(), xfer.delay());
    if (xfer.amount() < 0) {
      if (key != nullptr) {
        throw std::runtime_error("More than one send transfer not supported.");
      }
      key = keys.getKey(address);
    }
  }

  if (key == nullptr) {
    throw std::runtime_error("Exchange transactions must have one transfer with a negative amount.");
  }

  std::vector<byte> nonce(transaction.nonce().begin(), transaction.nonce().end());

  if (do_sign) {
    Tier2Transaction t2tx(
        operation,
        transfers,
        nonce,
        key,
        keys);
    EC_KEY_free(key);
    return t2tx.clone();
  } else {
    std::vector<byte> sig(transaction.sig().begin(), transaction.sig().end());
    Signature signature(sig);

    Tier2Transaction t2tx(
        operation,
        transfers,
        nonce,
        key,
        keys,
        signature);
    EC_KEY_free(key);
    return t2tx.clone();
  }
}

Tier2TransactionPtr CreateTransaction(const devv::proto::Transaction& transaction,
                                            std::string pk,
                                            std::string pk_pass) {
  auto operation = transaction.operation();
  auto pb_xfers = transaction.xfers();

  std::vector<Transfer> transfers;
  EC_KEY* key = nullptr;
  std::string pub_key;
  for (auto const& xfer : pb_xfers) {
    std::vector<byte> bytes(xfer.address().begin(), xfer.address().end());
    auto address = Address(bytes);
    transfers.emplace_back(address, xfer.coin(), xfer.amount(), xfer.delay());
    if (xfer.amount() < 0) {
      if (!pub_key.empty()) {
        throw std::runtime_error("More than one send transfer not supported.");
      }
      pub_key = address.getHexString();
      key = LoadEcKey(pub_key, pk, pk_pass);
    }
  }

  if (key == nullptr) {
    throw std::runtime_error("Exchange transactions must have one transfer with a negative amount.");
  }

  std::vector<byte> nonce(transaction.nonce().begin(), transaction.nonce().end());

  Tier2TransactionPtr t2tx_ptr = std::make_unique<Tier2Transaction>(
      operation,
      transfers,
      nonce,
      key);

  return t2tx_ptr;
}

TransactionPtr DeserializeTxProtobufString(const std::string& pb_tx, const KeyRing& keys, bool do_sign) {

  devv::proto::Transaction tx;
  tx.ParseFromString(pb_tx);

  auto t2tx_ptr = CreateTransaction(tx, keys, do_sign);

  return t2tx_ptr;
}

std::vector<TransactionPtr> ValidateOracle(oracleInterface& oracle
                                    , const Blockchain& chain
                                    , const KeyRing& keys) {
  std::vector<TransactionPtr> out;
  if (oracle.isValid(chain)) {
    std::map<uint64_t, std::vector<Tier2Transaction>> oracle_actions =
        oracle.getNextTransactions(chain, keys);
    for (auto& it : oracle_actions) {
      //TODO (nick) forward transactions for other shards to those shards
      for (auto& tx : it.second) {
        LOG_INFO << "Oracle: "+oracle.getOracleName()+" creates transaction: "+tx.getJSON();
        TransactionPtr t2tx_ptr = tx.clone();
        out.push_back(std::move(t2tx_ptr));
      }
    }
  }
  return out;
}

std::vector<TransactionPtr> DecomposeProposal(const devv::proto::Proposal& proposal,
                                              const Blockchain& chain,
                                              const KeyRing& keys) {
  std::vector<TransactionPtr> ptrs;
  std::string oracle_name = proposal.oraclename();
  if (oracle_name == CoinRequest::GetOracleName()) {
    CoinRequest oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  }else if (oracle_name == DoTransaction::GetOracleName()) {
    DoTransaction oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  /*} else if (oracle_name == api::GetOracleName()) {
    api oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else if (oracle_name == data::GetOracleName()) {
    data oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else if (oracle_name == dcash::GetOracleName()) {
    dcash oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));*/
  } else if (oracle_name == devvprotect::GetOracleName()) {
    TransactionPtr one_tx = DeserializeTxProtobufString(proposal.data(), keys, false);
    devvprotect oracle(Bin2Str(one_tx->getCanonical()));
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  /*} else if (oracle_name == dneroavailable::GetOracleName()) {
    dneroavailable oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else if (oracle_name == dnerowallet::GetOracleName()) {
    dnerowallet oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else if (oracle_name == id::GetOracleName()) {
    id oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else if (oracle_name == vote::GetOracleName()) {
    vote oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));*/
  } else if (oracle_name == revert::GetOracleName()) {
    revert oracle(proposal.data());
    std::vector<TransactionPtr> actions = ValidateOracle(oracle, chain, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin()), std::make_move_iterator(actions.end()));
  } else {
    LOG_ERROR << "Unknown oracle: " + oracle_name;
  }
  return ptrs;
}

std::vector<TransactionPtr> DeserializeEnvelopeProtobufString(
                              const std::string& pb_envelope
                            , const KeyRing& keys
                            , const Blockchain& context) {
  devv::proto::Envelope envelope;
  envelope.ParseFromString(pb_envelope);

  std::vector<TransactionPtr> ptrs;

  auto pb_transactions = envelope.txs();
  for (auto const& transaction : pb_transactions) {
    ptrs.push_back(CreateTransaction(transaction, keys));
  }

  auto pb_proposals = envelope.proposals();
  for (auto const& proposal : pb_proposals) {
    std::vector<TransactionPtr> actions = DecomposeProposal(proposal, context, keys);
    ptrs.insert(ptrs.end(), std::make_move_iterator(actions.begin())
        , std::make_move_iterator(actions.end()));
  }

  return ptrs;
}

devv::proto::AnnouncerResponse SerializeAnnouncerResponse(const AnnouncerResponsePtr& response_ptr) {
  devv::proto::AnnouncerResponse response;
  response.set_return_code(response_ptr->return_code);
  response.set_message(response_ptr->message);
  for (auto const& pending_ptr : response_ptr->pending) {
    devv::proto::PendingTransaction* one_pending_tx = response.add_txs();
    std::string raw_sig(std::begin(pending_ptr->sig.getCanonical())
        , std::end(pending_ptr->sig.getCanonical()));
    one_pending_tx->set_signature(raw_sig);
    one_pending_tx->set_expect_block(pending_ptr->expect_block);
    one_pending_tx->set_shard_index(pending_ptr->shard_index);
  }
  return response;
}

RepeaterRequestPtr DeserializeRepeaterRequest(const std::string& pb_request) {
  devv::proto::RepeaterRequest incoming_request;
  incoming_request.ParseFromString(pb_request);

  RepeaterRequest request;
  request.timestamp = incoming_request.timestamp();
  request.operation = incoming_request.operation();
  request.uri = incoming_request.uri();
  return std::make_unique<RepeaterRequest>(request);
}

devv::proto::RepeaterResponse SerializeRepeaterResponse(const RepeaterResponsePtr& response_ptr) {
  devv::proto::RepeaterResponse response;
  response.set_request_timestamp(response_ptr->request_timestamp);
  response.set_operation(response_ptr->operation);
  response.set_return_code(response_ptr->return_code);
  response.set_message(response_ptr->message);
  std::string raw_str(std::begin(response_ptr->raw_response)
      , std::end(response_ptr->raw_response));
  response.set_raw_response(raw_str);
  return response;
}

ServiceRequestPtr DeserializeServiceRequest(const std::string& pb_request) {
  devv::proto::ServiceRequest incoming_request;
  incoming_request.ParseFromString(pb_request);

  ServiceRequest request;
  request.timestamp = incoming_request.timestamp();
  request.endpoint = incoming_request.endpoint();
  auto pb_args = incoming_request.args();
  for (auto const& one_arg : pb_args) {
    std::pair<std::string, std::string> arg_pair(one_arg.key(), one_arg.value());
    request.args.insert(arg_pair);
  }
  return std::make_unique<ServiceRequest>(request);
}

devv::proto::ServiceResponse SerializeServiceResponse(const ServiceResponsePtr& response_ptr) {
  devv::proto::ServiceResponse response;
  response.set_request_timestamp(response_ptr->request_timestamp);
  response.set_endpoint(response_ptr->endpoint);
  response.set_return_code(response_ptr->return_code);
  response.set_message(response_ptr->message);
  for (auto const& one_arg : response_ptr->args) {
    devv::proto::KeyValuePair* pb_arg = response.add_args();
    pb_arg->set_key(one_arg.first);
    pb_arg->set_value(one_arg.second);
  }
  return response;
}

devv::proto::Transaction SerializeTransaction(const Tier2Transaction& one_tx, devv::proto::Transaction& tx) {
  tx.set_operation(static_cast<devv::proto::eOpType>(one_tx.getOperation()));
  std::vector<byte> nonce = one_tx.getNonce();
  std::string nonce_str(std::begin(nonce), std::end(nonce));
  tx.set_nonce(nonce_str);
  for (auto const& xfer : one_tx.getTransfers()) {
    devv::proto::Transfer* transfer = tx.add_xfers();
    std::string addr(std::begin(xfer->getAddress().getCanonical())
        ,std::end(xfer->getAddress().getCanonical()));
    transfer->set_address(addr);
    transfer->set_coin(xfer->getCoin());
    transfer->set_amount(xfer->getAmount());
    transfer->set_delay(xfer->getDelay());
  }
  Signature sig = one_tx.getSignature();
  std::string raw_sig(std::begin(sig.getCanonical())
      , std::end(sig.getCanonical()));
  tx.set_sig(raw_sig);
  return tx;
}

devv::proto::FinalBlock SerializeFinalBlock(const FinalBlock& block) {
  devv::proto::FinalBlock final_block;
  final_block.set_version(block.getVersion());
  final_block.set_num_bytes(block.getNumBytes());
  final_block.set_block_time(block.getBlockTime());
  std::string prev_hash_str(std::begin(block.getPreviousHash()), std::end(block.getPreviousHash()));
  final_block.set_prev_hash(prev_hash_str);
  std::string merkle_str(std::begin(block.getMerkleRoot()), std::end(block.getMerkleRoot()));
  final_block.set_merkle_root(merkle_str);
  final_block.set_tx_size(block.getSizeofTransactions());
  final_block.set_sum_size(block.getSummarySize());
  final_block.set_val_count(block.getNumValidations());
  for (auto const& one_tx : block.getRawTransactions()) {
    InputBuffer buffer(one_tx);
    devv::proto::Transaction* tx = final_block.add_txs();
    devv::proto::Transaction pbuf_tx = SerializeTransaction(Tier2Transaction::QuickCreate(buffer), *tx);
  }
  std::vector<byte> summary(block.getSummary().getCanonical());
  std::string summary_str(std::begin(summary), std::end(summary));
  final_block.set_summary(summary_str);
  std::vector<byte> vals(block.getValidation().getCanonical());
  std::string vals_str(std::begin(vals), std::end(vals));
  final_block.set_vals(vals_str);
  return final_block;
}

devv::proto::Envelope SerializeEnvelopeFromBinaryTransactions(const std::vector<std::vector<byte>>& txs) {
  devv::proto::Envelope envelope;
  for (auto const& one_tx : txs) {
    InputBuffer buffer(one_tx);
    devv::proto::Transaction* tx = envelope.add_txs();
    devv::proto::Transaction pbuf_tx = SerializeTransaction(Tier2Transaction::QuickCreate(buffer), *tx);
  }

  return envelope;
}

} // namespace Devv
