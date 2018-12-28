/*
 * blockchain_request_handlers.h
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <boost/filesystem.hpp>

#include "pbuf/devv_pbuf.h"
#include "primitives/blockchain.h"
#include "primitives/block_tools.h"

namespace Devv {

/**
 * Read a persisted block
 *
 * @param chain
 * @param shard_name
 * @param block
 * @param working_dir
 * @return
 */
std::vector<byte> ReadBlock(const Blockchain& chain,
                            const std::string& shard_name,
                            size_t block,
                            const boost::filesystem::path& working_dir);

/**
 *
 * @param shard_path
 * @param segment_index
 * @param segment_height
 * @return
 */
std::vector<byte> ReadBlock(const boost::filesystem::path& shard_path, size_t segment_index, size_t segment_height);

/**
 *
 * @param args
 * @param key
 * @return
 */
std::string GetServiceArgument(std::map<std::string, std::string>& args, const std::string& key);

/**
 * Checks if this shard has been persisted at this location
 *
 * @param shard The shard to look for
 * @param working_dir The working directory to search in
 * @return
 */
bool HasShard(const std::string& shard, const std::string& working_dir);

/**
 * Checks if a block exists at this location
 *
 * @param chain Current block chain
 * @param shard_name Current shard
 * @param block Block number
 * @param working_dir Working directory of blockchain
 * @return
 */
bool HasBlock(const Blockchain& chain,
              const std::string& shard_name,
              size_t block,
              const boost::filesystem::path& working_dir);

/**
 * Checks if a block exists at this location
 *
 * @param shard_path The path to the blockchain for this shard
 * @param segment_index The segment number of this block
 * @param segment_height The segment height of this block
 * @return
 */
bool HasBlock(const boost::filesystem::path& shard_path, size_t segment_index, size_t segment_height);

/**
 *
 * @param shard_name
 * @param start_block
 * @param end_block
 * @param target
 * @param working_dir
 * @param chain
 * @return
 */
std::map<std::string, std::string> TraceTransactions(const std::string& shard_name,
                                                     uint32_t start_block,
                                                     uint32_t end_block,
                                                     const std::vector<byte>& target,
                                                     const boost::filesystem::path& working_dir,
                                                     const Blockchain& chain);

/**
 *
 * @param shard_name
 * @param start_block
 * @param end_block
 * @param target
 * @param working_dir
 * @param chain
 * @return
 */
std::map<std::vector<byte>, std::vector<byte>> TraceTransactions_Binary(
                                                     const std::string& shard_name,
                                                     uint32_t start_block,
                                                     uint32_t end_block,
                                                     const std::vector<byte>& target,
                                                     const boost::filesystem::path& working_dir,
                                                     const Blockchain& chain);

/**
 * Dispatches incoming query requests.
 * ServiceRequestEventHandle is designed for threaded use. The
 * data members are const so they can be written once and then
 * accessed simultaneously from multiple threads. The concept is to
 * either have one ServiceRequestHandler that is shared between
 * multiple threads or multiple instances with a copy for each thread.
 */
class ServiceRequestEventHandler {
 public:
  /**
   * Consturctor
   * @param working_dir
   * @param shard_name
   */
  ServiceRequestEventHandler(const boost::filesystem::path& working_dir,
                             const std::string& shard_name);

  /**
   * Dispatch a ServiceRequest blockchain query. This method
   * will determine the type of request and will trigger the
   * appropriate handler.
   *
   * @param chain The current blockchain
   * @param request A request for blockchain information
   * @param response The current response in which to append info
   * @return A pointer to the updated response object
   */
  ServiceResponsePtr dispatchRequest(const Blockchain& chain,
                                     const ServiceRequest& request,
                                     const ServiceResponse& response);

  /**
   * Handle requests for block information
   *
   * @param chain The current blockchain
   * @param request The active request
   * @param response The response to be appended to
   * @return
   */
  ServiceResponsePtr handleBlockInfoRequest(const Blockchain& chain,
                                            ServiceRequestPtr request,
                                            ServiceResponsePtr response);

  /**
   * Handle requests for shard information
   *
   * @param chain The current blockchain
   * @param request The active request
   * @param response The response to be appended to
   * @return
   */
  ServiceResponsePtr handleShardInfoRequest(const Blockchain& chain,
                                            ServiceRequestPtr request,
                                            ServiceResponsePtr response);

  /**
   * Handle requests to trace wallets/addresses
   *
   * @param chain The current blockchain
   * @param request The active request
   * @param response The response to be appended to
   * @return
   */
  ServiceResponsePtr handleTraceRequest(const Blockchain& chain,
                                        ServiceRequestPtr request,
                                        ServiceResponsePtr response);

  /**
   * Handle requests for transaction information
   *
   * @param chain The current blockchain
   * @param request The active request
   * @param response The response to be appended to
   * @return
   */
  ServiceResponsePtr handleTxInfoRequest(const Blockchain& chain,
                                         ServiceRequestPtr request,
                                         ServiceResponsePtr response);

  /**
   * Handle requests for raw block data
   *
   * @param chain
   * @param request
   * @param response
   * @return
   */
  ServiceResponsePtr handleBlockQueryRequest(const Blockchain& chain,
                                             ServiceRequestPtr request,
                                             ServiceResponsePtr response);

 private:
  /// The working directory containing the blockchain
  const boost::filesystem::path working_dir_;
  /// The current shard name
  const std::string shard_name_;
};

/**
 * Handle service request operations.
 * See the Repeater Interface page of the wiki for further information.
 *
 * @param request - a smart pointer to the request
 * @param working_dir - the directory where blocks are stored
 * @param shard_name - the name of the shard this process tracks
 * @param shard_index - the index of the shard this process tracks
 * @param chain - the blockchain this shard tracks
 * @return ServiceResponsePtr - a smart pointer to the response
 */
ServiceResponsePtr HandleServiceRequest(ServiceRequestEventHandler& event_handler,
                                        const ServiceRequestPtr request,
                                        const std::string& working_dir,
                                        const std::string& shard_name,
                                        unsigned int shard_index,
                                        const Blockchain& chain);


} // namespace Devv
