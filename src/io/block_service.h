/*
 * query_service.h
 *
 * <description>
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "io/zhelpers.hpp"

#include "primitives/Transaction.h"
#include "primitives/FinalBlock.h"
#include "primitives/blockchain.h"

#include "pbuf/devv_pbuf.h"

namespace Devv {

/**
 * Interface class for communication with devv-query
 */
class BlockReaderInterface {
 public:
  BlockReaderInterface()
  {
  }

  virtual ~BlockReaderInterface() {};

  virtual void open() = 0;

  virtual void close() = 0;
  virtual FinalBlockUniquePtr getBlockAt(const ChainState& prior, const KeyRing& keys, size_t block_number) = 0;

 private:

};


/**
 * Interface class for communication with devv-query
 */
class DevvQueryBlockReader : public BlockReaderInterface {
 public:
  DevvQueryBlockReader();

  DevvQueryBlockReader(zmq::context_t& zmq_context,
                       const std::string& query_uri,
                       const std::string& shard_name)
      : zmq_context_(zmq_context)
      , socket_(zmq_context_, ZMQ_REQ)
      , query_uri_(query_uri)
      , shard_name_(shard_name)
  {
  }

  void open();

  FinalBlockUniquePtr getBlockAt(const ChainState& prior, const KeyRing& keys, size_t block_number);

  std::string getURI() const { return query_uri_;}
 private:
  zmq::context_t& zmq_context_;
  zmq::socket_t socket_;

  std::string query_uri_;
  std::string shard_name_;

  eAppMode mode_;
};

} // namespace Devv
