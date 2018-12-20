/*
 * message_service.cpp Sends and receives Devv messages using ZMQ.
 *
 * @copywrite  2018 Devvio Inc
 */
#include <io/message_service.h>

#include <vector>

#include <io/constants.h>

namespace Devv {
namespace io {

TransactionServer::TransactionServer(zmq::context_t& context, const std::string& bind_url)
    : bind_url_(bind_url), context_(context) {}

void TransactionServer::sendMessage(DevvMessageUniquePtr dc_message) noexcept {
  if (!keep_running_) {
    LOG_WARNING << "sendMessage(): Won't send message: !keep_running!";
    return;
  }
  MTR_SCOPE_FUNC();
  LOG_DEBUG << "sendMessage(): Sending message: [" << dc_message->index << ", " << GetMessageType(*dc_message) << ", "
            << dc_message->uri << "]";

  auto buffer = serialize(*dc_message);
  s_sendmore(*pub_socket_, dc_message->uri);
  s_send(*pub_socket_, buffer);
}

void TransactionServer::queueMessage(DevvMessageUniquePtr message) noexcept {
  LOG_DEBUG << "queueMessage(): Queue: [" << message->index << ", " << GetMessageType(*message) << ", " << message->uri
            << "]";
  message_queue_.push(std::move(message));
}

void TransactionServer::startServer() {
  LOG_DEBUG << "Starting TransactionServer";
  if (keep_running_) {
    LOG_WARNING << "Attempted to start a TransactionServer that was already running";
    return;
  }
  server_thread_ = std::make_unique<std::thread>([this]() { this->run(); });
  keep_running_ = true;
}

void TransactionServer::stopServer() {
  LOG_DEBUG << "Stopping TransactionServer";
  if (keep_running_) {
    keep_running_ = false;
    message_queue_.ClearBlockers();
    server_thread_->join();
    LOG_INFO << "Stopped TransactionServer";
  } else {
    LOG_WARNING << "Attempted to stop a stopped server!";
  }
}

void TransactionServer::run() noexcept {
  MTR_META_THREAD_NAME("TransactionServer::run() Thread");
  pub_socket_ = std::make_unique<zmq::socket_t>(context_, ZMQ_PUB);
  LOG_INFO << "Server: Binding bind_url_ '" << bind_url_ << "'";
  pub_socket_->bind(bind_url_);

  for (;;) {
    auto message = message_queue_.pop();
    if (!message) {
      LOG_ERROR << "TransactionServer::pop()ped a nullptr - exiting thread";
      break;
    }
    sendMessage(std::move(message));
    if (server_thread_ && !keep_running_) { break; }
  }
}

/**
 * Communication class. Register a callback using the registerCallback() method
 * and connect to one or more TransactionServers to receive DevvMessages
 */
TransactionClient::TransactionClient(zmq::context_t& context)
    : peer_urls_(), context_(context), sub_socket_(nullptr), callback_() {}

void TransactionClient::addConnection(const std::string& endpoint) { peer_urls_.push_back(endpoint); }

void TransactionClient::processIncomingMessage() noexcept {
  LOG_TRACE << "processIncomingMessage(): Waiting for message";
  /* Block until a message is available to be received from socket */

  auto uri = s_recv(*sub_socket_);
  if (uri == "") { return; }

  LOG_DEBUG << "Received - envelope message: " << uri;
  auto mess = s_vrecv(*sub_socket_);
  if (mess.size() == 0) { return; }

  auto devv_message = deserialize(mess);
  LOG_DEBUG << "processIncomingMessage(): Received [" << devv_message->index << ", "
            << GetMessageType(*devv_message) << ", " << devv_message->uri << "]";
  MTR_INSTANT_FUNC();

  LogDevvMessageSummary(*devv_message, "processIncomingMessage");

  callback_(std::move(devv_message));
}

void TransactionClient::run() {
  try {
    MTR_META_THREAD_NAME("TransactionClient::run() Thread");
    sub_socket_ = std::unique_ptr<zmq::socket_t>(new zmq::socket_t(context_, ZMQ_SUB));
    int timeout_ms = 100;
    sub_socket_->setsockopt(ZMQ_RCVTIMEO, &timeout_ms, sizeof(timeout_ms));

    for (auto endpoint : peer_urls_) {
      sub_socket_->connect(endpoint);
      for (auto filter : filter_vector_) {
        LOG_DEBUG << "ZMQ_SUBSCRIBE: '" << endpoint << ":" << filter << "'";
        sub_socket_->setsockopt(ZMQ_SUBSCRIBE, filter.c_str(), filter.size());
      }
    }

    for (;;) {
      processIncomingMessage();
      if (client_thread_ && !keep_running_) { break; }
    }
  } catch (const std::exception& e) {
    LOG_FATAL << "EXCEPTION[TransactionClient::run()]:"+std::string(e.what());
  }
}

void TransactionClient::startClient() {
  LOG_DEBUG << "Starting TransactionClient thread";
  if (keep_running_) {
    LOG_WARNING << "Attempted to start a TransactionClient that was already running";
    return;
  }
  client_thread_ = std::make_unique<std::thread>([this]() { this->run(); });
  keep_running_ = true;
}

void TransactionClient::stopClient() {
  if (keep_running_) {
    LOG_DEBUG << "Stopping TransactionClient";
    keep_running_ = false;
    if (client_thread_) {
      client_thread_->join();
      client_thread_ = nullptr;
    }
    LOG_INFO << "Stopped TransactionClient";
  } else {
    LOG_WARNING << "Attempted to stop a stopped TransactionClient!";
  }
}

void TransactionClient::listenTo(const std::string& filter) { filter_vector_.push_back(filter); }

void TransactionClient::attachCallback(DevvMessageCallback callback) { callback_ = callback; }

std::unique_ptr<io::TransactionClient> CreateTransactionClient(const std::vector<std::string>& host_vector,
                                                               zmq::context_t& context) {
  std::unique_ptr<io::TransactionClient> client(new io::TransactionClient(context));
  for (auto i : host_vector) {
    client->addConnection(i);
  }
  return client;
}

std::unique_ptr<io::TransactionServer> CreateTransactionServer(const std::string& bind_endpoint,
                                                               zmq::context_t& context) {
  std::unique_ptr<io::TransactionServer> server(new io::TransactionServer(context,
                                                                          bind_endpoint));
  return server;
}

}  // namespace io
}  // namespace Devv
