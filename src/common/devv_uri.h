/*
 * devv_uri.h provides tools for Devv URI strings.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef DEVV_URI_H
#define DEVV_URI_H

#include <boost/algorithm/string.hpp>

#include "common/binary_converters.h"
#include "primitives/Address.h"
#include "primitives/Signature.h"

namespace Devv {

static const std::string kDEVV_PROTOCOL = "devv://";

struct DevvUri {
  bool valid = false;
  std::string shard;
  uint32_t block_height = UINT32_MAX;
  Signature sig;
  Address addr;
};

static std::string MakeDevvUri(std::string shard_name) {
  std::string uri = kDEVV_PROTOCOL+shard_name;
  return uri;
}

static std::string MakeDevvUri(std::string shard_name, uint32_t block_height) {
  std::string uri = kDEVV_PROTOCOL+shard_name+"/"+std::to_string(block_height);
  return uri;
}

static std::string MakeDevvUri(std::string shard_name, uint32_t block_height
                            , Signature sig) {
  std::string uri = kDEVV_PROTOCOL+shard_name+"/"+std::to_string(block_height);
  uri += "/"+sig.getJSON();
  return uri;
}

static std::string MakeDevvUri(std::string shard_name, uint32_t block_height
                            , Signature sig, Address addr) {
  std::string uri = kDEVV_PROTOCOL+shard_name+"/"+std::to_string(block_height);
  uri += "/"+sig.getJSON();
  uri += "/"+addr.getJSON();
  return uri;
}

static std::string MakeDevvUri(DevvUri uri_struct) {
  std::string uri;
  if (!uri_struct.valid) return uri;
  if (uri_struct.shard.empty()) return uri;
  uri = kDEVV_PROTOCOL+uri_struct.shard;
  if (uri_struct.block_height == UINT32_MAX) return uri;
  uri += "/"+std::to_string(uri_struct.block_height);
  if (uri_struct.sig.isNull()) return uri;
  uri += "/"+uri_struct.sig.getJSON();
  if (uri_struct.addr.isNull()) return uri;
  uri += "/"+uri_struct.addr.getJSON();
  return uri;
}

static DevvUri ParseDevvUri(std::string uri) {
  DevvUri devv_uri;
  size_t pos = uri.find(kDEVV_PROTOCOL);
  if (pos != 0) return devv_uri;
  pos += kDEVV_PROTOCOL.size();

  std::vector<std::string> parts;
  std::string uri_path = uri.substr(pos);
  boost::split(parts, uri_path, boost::is_any_of("/"));
  size_t num_parts = parts.size();
  if (num_parts > 0) {
    if (!parts[0].empty()) devv_uri.shard = trim(parts[0]);
  }
  if (num_parts > 1) {
    if (!parts[1].empty()) {
      devv_uri.block_height = std::stoul(trim(parts[1]), nullptr, 10);
    }
  }
  if (num_parts > 2) {
    if (!parts[2].empty()) {
      devv_uri.sig = Signature(Hex2Bin(trim(parts[2])));
    }
  }
  if (num_parts > 3) {
    if (!parts[3].empty()) {
      devv_uri.addr = Address(Hex2Bin(trim(parts[3])));
    }
  }
  devv_uri.valid = true;
  return devv_uri;
}

}  // namespace Devv
#endif  // DEVV_URI_H
