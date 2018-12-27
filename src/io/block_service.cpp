/*
 * query_service.cpp
 *
 * <description>
 *
 * @copywrite  2018 Devvio Inc
 */

#include "io/block_service.h"

namespace Devv {

void DevvQueryBlockReader::open() {
  zmq::socket_t socket(zmq_context_, ZMQ_REQ);
  socket.setsockopt(ZMQ_LINGER, 0);
  socket.setsockopt(ZMQ_RCVTIMEO, 2000);
  socket.connect(query_uri_);
}

FinalBlockUniquePtr DevvQueryBlockReader::getBlockAt(const ChainState& prior, const KeyRing& keys, size_t block_number) {

  ServiceRequestPtr request;
  request->timestamp = GetMillisecondsSinceEpoch();

  auto pbuf_request = SerializeServiceRequest(std::move(request));
  auto request_string = pbuf_request.SerializeAsString();
  zmq::message_t request_message(request_string.size());
  memcpy(request_message.data(), request_string.data(), request_string.size());
  socket_.send(request_message);

  auto vec_in = s_vrecv(socket_);

  //zmq::message_t reply_message;
  //socket_.recv(&reply_message);
  //reply_message.data();
  InputBuffer buffer(vec_in);

  FinalBlockUniquePtr block = std::make_unique<FinalBlock>(buffer, prior, keys, mode_);
  return block;
}

} // namespace Devv
