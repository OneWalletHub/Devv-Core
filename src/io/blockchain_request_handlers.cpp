/*
 * blockchain_request_handlers.cpp
 *
 * @copywrite  2018 Devvio Inc
 */
#include "io/blockchain_request_handlers.h"

namespace Devv {

namespace fs = boost::filesystem;

std::vector<byte> ReadBlock(const Blockchain& chain,
                            const std::string& shard_name,
                            size_t block,
                            const boost::filesystem::path& working_dir) {
  fs::path shard_path(fs::path(working_dir) / shard_name);
  return ReadBlock(shard_path, chain.getSegmentIndexAt(block), chain.getSegmentHeightAt(block));
}

std::vector<byte> ReadBlock(const fs::path& shard_path, size_t segment_index, size_t segment_height) {

  std::vector<byte> out;
  auto block_path = GetBlockPath(shard_path, segment_index, segment_height);
  std::ifstream block_file(block_path.string(), std::ios::in | std::ios::binary);

  block_file.unsetf(std::ios::skipws);

  std::streampos block_size;
  block_file.seekg(0, std::ios::end);
  block_size = block_file.tellg();
  block_file.seekg(0, std::ios::beg);
  out.reserve(block_size);

  out.insert(out.begin(),
             std::istream_iterator<byte>(block_file),
             std::istream_iterator<byte>());
  return out;
}

std::string GetServiceArgument(std::map<std::string, std::string>& args, const std::string& key) {
  auto it = args.find(key);
  if (it != args.end()) {
    return it->second;
  }
  return std::string();
}

bool HasShard(const std::string& shard, const std::string& working_dir) {
  std::string shard_dir(working_dir+fs::path::preferred_separator+shard);
  fs::path dir_path(shard_dir);
  return fs::is_directory(dir_path);
}

bool HasBlock(const Blockchain& chain,
              const std::string& shard_name,
              size_t block,
              const boost::filesystem::path& working_dir) {
  return(fs::exists(GetStandardBlockPath(chain, shard_name, working_dir, block)));
}

bool HasBlock(const fs::path& shard_path, size_t segment_index, size_t segment_height) {
  return(fs::exists(GetBlockPath(shard_path, segment_index, segment_height)));
}

std::map<std::string, std::string> TraceTransactions(const std::string& shard_name,
                                                     size_t start_block,
                                                     size_t end_block,
                                                     const std::vector<byte>& target,
                                                     const boost::filesystem::path& working_dir,
                                                     const Blockchain& chain) {
  std::map<std::string, std::string> txs;
  size_t highest = std::min(end_block, chain.size());
  if (highest < start_block) return txs;

  for (uint32_t i=start_block; i<=highest; ++i) {
    std::vector<byte> block = ReadBlock(chain, shard_name, i, working_dir);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    for (const auto& raw_tx : one_block.getRawTransactions()) {
      if(std::search(std::begin(raw_tx), std::end(raw_tx)
          , std::begin(target), std::end(target)) != std::end(raw_tx)) {
        InputBuffer t2_buffer(raw_tx);
        Tier2Transaction t2tx = Tier2Transaction::QuickCreate(t2_buffer);
        txs.insert(std::make_pair(ToHex(t2tx.getSignature().getCanonical()), ToHex(raw_tx)));
      }
    }
  }
  return txs;
}

ServiceRequestEventHandler::ServiceRequestEventHandler(const fs::path& working_dir,
                                                       const std::string& shard_name)
    : working_dir_(working_dir)
    , shard_name_(shard_name)
{
}

std::map<std::vector<byte>, std::vector<byte>> TraceTransactions_Binary(
                                                     const std::string& shard_name,
                                                     size_t start_block,
                                                     size_t end_block,
                                                     const std::vector<byte>& target,
                                                     const boost::filesystem::path& working_dir,
                                                     const Blockchain& chain) {
  std::map<std::vector<byte>, std::vector<byte>> txs;
  size_t highest = std::min(end_block, chain.size());
  if (highest < start_block) return txs;

  for (uint32_t i=start_block; i<=highest; ++i) {
    std::vector<byte> block = ReadBlock(chain, shard_name, i, working_dir);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    for (const auto& raw_tx : one_block.getRawTransactions()) {
      if(std::search(std::begin(raw_tx), std::end(raw_tx)
          , std::begin(target), std::end(target)) != std::end(raw_tx)) {
        InputBuffer t2_buffer(raw_tx);
        Tier2Transaction t2tx = Tier2Transaction::QuickCreate(t2_buffer);
        txs.insert(std::make_pair(t2tx.getSignature().getCanonical(), raw_tx));
      }
    }
  }
  return txs;
}

ServiceResponsePtr ServiceRequestEventHandler::dispatchRequest(const Blockchain& chain,
                                                               const ServiceRequest& request,
                                                               const ServiceResponse& response) {

  auto response_ptr = std::make_unique<ServiceResponse>(response);
  auto request_ptr = std::make_unique<ServiceRequest>(request);

  LOG_DEBUG << "dipatchRequest(): handling " << request.endpoint;
  if (SearchString(request.endpoint, "/block-info", true)) {
    response_ptr = handleBlockInfoRequest(chain, std::move(request_ptr), std::move(response_ptr));
  } else if (SearchString(request.endpoint, "/shard-info", true)) {
    response_ptr = handleShardInfoRequest(chain, std::move(request_ptr), std::move(response_ptr));
  } else if (SearchString(request.endpoint, "/trace", true)) {
    response_ptr = handleTraceRequest(chain, std::move(request_ptr), std::move(response_ptr));
  } else if (SearchString(request.endpoint, "/tx-info", true)) {
    response_ptr = handleTxInfoRequest(chain, std::move(request_ptr), std::move(response_ptr));
  } else if (SearchString(request.endpoint, "/block-query", true)) {
    response_ptr = handleBlockQueryRequest(chain, std::move(request_ptr), std::move(response_ptr));
  }
  return response_ptr;
}

ServiceResponsePtr ServiceRequestEventHandler::handleBlockInfoRequest(const Blockchain& chain,
                                          ServiceRequestPtr request,
                                          ServiceResponsePtr response) {
  auto height = std::stoul(GetServiceArgument(request->args, "block-height"));
  LOG_DEBUG << "handleBlockInfoRequest(): " << height;
  response->args.insert(std::make_pair("block-height", std::to_string(height)));
  if (height < chain.size()) {
    std::vector<byte> block = chain.raw_at(height);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    response->args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
    response->args.insert(std::make_pair("tx-size", std::to_string(one_block.getSizeofTransactions())));
    response->args.insert(std::make_pair("num-txs", std::to_string(one_block.getNumTransactions())));
    response->args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
    response->args.insert(std::make_pair("block-volume", std::to_string(one_block.getVolume())));
    response->args.insert(std::make_pair("Merkle", ToHex(one_block.getMerkleRoot())));
    response->args.insert(std::make_pair("previous-hash", ToHex(one_block.getPreviousHash())));
  } else if (HasBlock(chain, shard_name_, height, working_dir_)) {
    std::vector<byte> block = ReadBlock(chain, shard_name_, height, working_dir_);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    response->args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
    response->args.insert(std::make_pair("tx-size", std::to_string(one_block.getSizeofTransactions())));
    response->args.insert(std::make_pair("num-txs", std::to_string(one_block.getNumTransactions())));
    response->args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
    response->args.insert(std::make_pair("block-volume", std::to_string(one_block.getVolume())));
    response->args.insert(std::make_pair("Merkle", ToHex(one_block.getMerkleRoot())));
    response->args.insert(std::make_pair("previous-hash", ToHex(one_block.getPreviousHash())));
  } else {
    response->return_code = 1050;
    response->message = "Block "+std::to_string(height)+" is not available.";
  }
  return response;
}

ServiceResponsePtr ServiceRequestEventHandler::handleShardInfoRequest(const Blockchain& chain,
                                          ServiceRequestPtr request,
                                          ServiceResponsePtr response) {
  auto highest = chain.size();
  LOG_DEBUG << "handleShardInfoRequest(): " << highest;
  response->args.insert(std::make_pair("current-block", std::to_string(highest)));
  if (highest > 0) {
    std::shared_ptr<const FinalBlock> top_block = chain.back();
    response->args.insert(std::make_pair("last-block-time"
        , std::to_string(top_block->getBlockTime())));
    response->args.insert(std::make_pair("last-tx-count"
        , std::to_string(top_block->getNumTransactions())));
    size_t total_txs = chain.getNumTransactions();
    response->args.insert(std::make_pair("total-tx-count"
        , std::to_string(total_txs)));
    response->args.insert(std::make_pair("avg-tx-per-block"
        , std::to_string(total_txs/highest)));
    response->args.insert(std::make_pair("avg-block-time"
        , std::to_string(chain.getAvgBlocktime())));
  }
  return response;
}

ServiceResponsePtr ServiceRequestEventHandler::handleTraceRequest(const Blockchain& chain,
                                      ServiceRequestPtr request,
                                      ServiceResponsePtr response) {
  std::string start_str = GetServiceArgument(request->args, "start-block");
  LOG_DEBUG << "handleTraceRequest(): " << start_str;
  size_t start_block = 0;
  if (!start_str.empty()) {
    start_block = std::stoi(start_str);
    response->args.insert(std::make_pair("start-block", start_str));
  }
  std::string end_str = GetServiceArgument(request->args, "end-block");
  size_t end_block = UINT32_MAX;
  if (!end_str.empty()) {
    end_block = std::stoi(end_str);
    response->args.insert(std::make_pair("end-block", end_str));
  }
  std::string sig = GetServiceArgument(request->args, "signature");
  std::string addr = GetServiceArgument(request->args, "address");
  if (sig.empty() && addr.empty()) {
    response->return_code = 1050;
    response->message = "An address or signature is required for a trace.";
  }
  if (!sig.empty()) {
    response->args.insert(std::make_pair("signature", sig));
    std::vector<byte> target = Hex2Bin(sig);
    std::map<std::string, std::string>
        txs = TraceTransactions(shard_name_, start_block, end_block, target, working_dir_, chain);
    response->args.insert(txs.begin(), txs.end());
  }
  if (!addr.empty()) {
    response->args.insert(std::make_pair("address", addr));
    std::vector<byte> target = Hex2Bin(addr);
    std::map<std::string, std::string>
        txs = TraceTransactions(shard_name_, start_block, end_block, target, working_dir_, chain);
    response->args.insert(txs.begin(), txs.end());
  }
  return response;
}

ServiceResponsePtr ServiceRequestEventHandler::handleTxInfoRequest(const Blockchain& chain,
                                       ServiceRequestPtr request,
                                       ServiceResponsePtr response) {
  size_t height = std::stoi(GetServiceArgument(request->args, "block-height"));
  LOG_DEBUG << "handleTxInfoRequest(): " << height;
  response->args.insert(std::make_pair("block-height", std::to_string(height)));
  std::string sig = GetServiceArgument(request->args, "signature");
  response->args.insert(std::make_pair("signature", sig));
  Signature tx_id(Hex2Bin(sig));
  if (height < chain.size()) {
    std::vector<byte> block = chain.raw_at(height);
    InputBuffer buffer(block);
    ChainState state;
    bool found_tx = false;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    for (const auto& tx : one_block.getTransactions()) {
      if (tx->getSignature() == tx_id) {
        response->args.insert(std::make_pair("tx-data", ToHex(tx->getCanonical())));
        found_tx = true;
        break;
      }
    }
    if (!found_tx) {
      response->return_code = 1050;
      response->message = "Transaction not found in block "+std::to_string(height)+".";
    }
  } else if (HasBlock(chain, shard_name_, height, working_dir_)) {
    std::vector<byte> block = ReadBlock(chain, shard_name_, height, working_dir_);
    InputBuffer buffer(block);
    ChainState state;
    bool found_tx = false;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    for (const auto& tx : one_block.getTransactions()) {
      if (tx->getSignature() == tx_id) {
        response->args.insert(std::make_pair("tx-data", ToHex(tx->getCanonical())));
        found_tx = true;
        break;
      }
    }
    if (!found_tx) {
      response->return_code = 1050;
      response->message = "Transaction not found in block "+std::to_string(height)+".";
    }
  } else {
    response->return_code = 1050;
    response->message = "Block "+std::to_string(height)+" is not available.";
  }
  return response;
}

ServiceResponsePtr ServiceRequestEventHandler::handleBlockQueryRequest(const Blockchain& chain,
                                                                      ServiceRequestPtr request,
                                                                      ServiceResponsePtr response) {
  auto height = std::stoul(GetServiceArgument(request->args, "block-height"));
  LOG_DEBUG << "handleBlockQueryRequest(): " << height;
  response->args.insert(std::make_pair("block-height", std::to_string(height)));
  if (height < chain.size()) {
    std::vector<byte> block = chain.raw_at(height);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    response->args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
    response->args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
    response->args.insert(std::make_pair("block-data", ToHex(block)));
  } else if (HasBlock(chain, shard_name_, height, working_dir_)) {
    std::vector<byte> block = ReadBlock(chain, shard_name_, height, working_dir_);
    InputBuffer buffer(block);
    ChainState state;
    FinalBlock one_block(FinalBlock::Create(buffer, state));
    response->args.insert(std::make_pair("block-time", std::to_string(one_block.getBlockTime())));
    response->args.insert(std::make_pair("block-size", std::to_string(one_block.getNumBytes())));
    response->args.insert(std::make_pair("block-data", ToHex(block)));
  } else {
    response->return_code = 1050;
    response->message = "Block "+std::to_string(height)+" is not available.";
  }
  return response;
}

ServiceResponsePtr HandleServiceRequest(ServiceRequestEventHandler& event_handler,
                                        const ServiceRequestPtr request,
                                        const std::string& working_dir,
                                        const std::string& shard_name,
                                        unsigned int shard_index,
                                        const Blockchain& chain) {
  ServiceResponse response;
  try {
    response.request_timestamp = request->timestamp;
    response.endpoint = request->endpoint;
    if (request->args.empty()) {
      response.return_code = 1010;
      response.message = "Missing arguments.";
      return std::make_unique<ServiceResponse>(response);
    }
    std::string name = GetServiceArgument(request->args, "shard-name");
    std::string shard_id = GetServiceArgument(request->args, "shard-id");
    if (shard_name != name && shard_id != std::to_string(shard_index)) {
      response.return_code = 1050;
      response.message = "Shard '"+name+"' is not available.";
      return std::make_unique<ServiceResponse>(response);
    } else if (name.empty() && !HasShard(shard_id, working_dir)) {
      response.return_code = 1050;
      response.message = "Shard "+shard_id+" is not available.";
      return std::make_unique<ServiceResponse>(response);
    } else if (shard_id.empty()) {
      //TODO(nick@devv.io): devv-query should be able to track multiple shards,
      //but to do that it needs a map shard_name->shard_index/id
      shard_id = std::to_string(shard_index);
      LOG_DEBUG << "shard_id empty, setting to: " << shard_index;
    }
    LOG_DEBUG << "HandleServiceRequest: shard_name: " << shard_name << " id: " << shard_id << " name: " << name;
    response.args.insert(std::make_pair("protocol-version", "0"));
    response.args.insert(std::make_pair("shard-name", shard_name));
    return event_handler.dispatchRequest(chain, *request, response);
  } catch (std::exception& e) {
    LOG_ERROR << "Devv-query Error: "+std::string(e.what());
    response.return_code = 1010;
    response.message = "Devv-query Error: "+std::string(e.what());
    return std::make_unique<ServiceResponse>(response);
  }
}

} // namespace Devv
