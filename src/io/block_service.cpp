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

} // namespace Devv
