/*
 * transaction_server.cpp - implements a server for connectivity tests
 * between T2 and T1 shards.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#include <unistd.h>
#include <chrono>
#include <thread>

#include "io/constants.h"
#include "io/message_service.h"
#include "types/DevvMessage.h"

#include "transaction_test_struct.h"

int main(int argc, char** argv) {
  auto dev_message = std::make_unique<Devv::DevvMessage>(10);
  dev_message->index = 10;
  dev_message->uri = "Hello!";
  dev_message->message_type = Devv::eMessageType::VALID;
  dev_message->data.emplace_back(25);

  Devv::LogDevvMessageSummary(*dev_message, "main()");

  auto buffer = Devv::serialize(*dev_message);

  auto new_message = Devv::deserialize(buffer);

  assert(dev_message->index == new_message->index);

  Devv::LogDevvMessageSummary(*new_message, "main2()");
  LOG_INFO << "We made it!!!";

  if (argc < 2) {
    LOG_ERROR << "Usage: " << argv[0] << " bind-endpoint [URI]";
    LOG_ERROR << "ex: " << argv[0] << " tcp://*:55557 my-message";
    exit(-1);
  }

  std::string bind_endpoint = argv[1];

  LOG_INFO << "Binding to " << bind_endpoint;

  std::string my_uri;
  if (argc > 2) {
    my_uri = argv[2];
  } else {
    my_uri = "my-uri";
  }

  test_struct test;
  test.a = 10;
  test.b = 20;
  test.c = 30;

  // Zmq Context
  zmq::context_t context(1);
  Devv::io::TransactionServer server{context, bind_endpoint};
  server.startServer();

  for (;;) {
    sleep(1);
    auto message = std::make_unique<Devv::DevvMessage>(10);
    message->index = 15;
    message->uri = my_uri;
    message->message_type = Devv::eMessageType::VALID;
    message->SetData(test);
    Devv::LogDevvMessageSummary(*message, "");

    // LOG_INFO << "Sending message: " << message;
    server.queueMessage(std::move(message));
  }

  return 0;
}
