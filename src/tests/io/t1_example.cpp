/*
 * t1_example.cpp - tests connectivity between T2 and T1 shards.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

#include "common/logger.h"
#include "io/constants.h"
#include "io/message_service.h"

void print_t1_devv_message(Devv::DevvMessageUniquePtr message) {
  LOG(info) << "Got a T1 message: "
            << "Devv->uri: " << message->uri;
}

void print_t2_devv_message(Devv::DevvMessageUniquePtr message) {
  LOG(info) << "Got a T2 message: "
            << "Devv->uri: " << message->uri;
}

namespace po = boost::program_options;

int main(int argc, char** argv) {
  std::string t1_bind_endpoint("");
  std::string t2_bind_endpoint("");

  std::vector<std::string> t1_host_vector{};
  std::vector<std::string> t2_host_vector{};

  try {
    po::options_description desc(
        "\n\
The t1_example program can be used to demonstrate connectivity of\n\
T1 and T2 networks. By default, it will send random DevvMessage\n\
every 5 seconds to any other t1_example programs that have connected\n\
to it will receive the message. If it receives a message, it will\n\
execute the appropriate T1 or T2 callback. t1_example can connect\n\
and listen to multiple other t1_example programs so almost any size\n\
network could be build and tested.\n\nAllowed options");
    desc.add_options()("help", "produce help message")(
        "t1-host-list", po::value<std::vector<std::string>>(),
        "T1 client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple hosts.")(
        "t2-host-list", po::value<std::vector<std::string>>(),
        "T2 client URI (i.e. tcp://192.168.10.1:5005). Option can be repeated to connect to multiple hosts.")(
        "t1-bind-endpoint", po::value<std::string>(), "Endpoint for T1 server (i.e. tcp://*:5556)")(
        "t2-bind-endpoint", po::value<std::string>(), "Endpoint for T2 server (i.e. tcp://*:5556)");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help")) {
      LOG(debug) << desc << "\n";
      return 0;
    }

    if (vm.count("t1-bind-endpoint")) {
      t1_bind_endpoint = vm["t1-bind-endpoint"].as<std::string>();
      LOG(debug) << "T1 bind URI: " << t1_bind_endpoint;
    } else {
      LOG(debug) << "T1 bind URI was not set.\n";
    }

    if (vm.count("t2-bind-endpoint")) {
      t2_bind_endpoint = vm["t2-bind-endpoint"].as<std::string>();
      LOG(debug) << "T2 bind URI: " << t2_bind_endpoint;
    } else {
      LOG(debug) << "T2 bind URI was not set.\n";
    }

    if (vm.count("t1-host-list")) {
      t1_host_vector = vm["t1-host-list"].as<std::vector<std::string>>();
      LOG(debug) << "T1 host URIs:";
      for (auto i : t1_host_vector) {
        LOG(debug) << "  " << i;
      }
    }
    if (vm.count("t2-host-list")) {
      t2_host_vector = vm["t2-host-list"].as<std::vector<std::string>>();
      LOG(debug) << "T2 host URIs:";
      for (auto i : t2_host_vector) {
        LOG(debug) << "  " << i;
      }
    }
  } catch (std::exception& e) {
    LOG_ERROR << "error: " << e.what() << "\n";
    return 1;
  } catch (...) {
    LOG_ERROR << "Exception of unknown type!\n";
  }

  std::vector<std::thread> all_threads{};

  // Zmq Context
  zmq::context_t context(1);

  // Setup the T1 client. This will connect to
  // other nodes
  Devv::io::TransactionClient t1_client{context};
  for (auto i : t1_host_vector) {
    t1_client.addConnection(i);
  }
  t1_client.attachCallback(print_t1_devv_message);

  std::thread t1_client_thread([&t1_client]() noexcept {
    LOG(info) << "Starting T1 client thread ...";
    t1_client.run();
    LOG(info) << "Client stopped.";
  });

  all_threads.emplace_back(std::move(t1_client_thread));

  typedef std::unique_ptr<Devv::io::TransactionServer> TSPtr;
  TSPtr t1_server;
  if (t1_bind_endpoint.size() > 0) {
    t1_server = TSPtr(new Devv::io::TransactionServer(context, t1_bind_endpoint));
    t1_server->startServer();
    std::thread t1_server_thread([&t1_server]() {
      for (;;) {
        sleep(5);
        auto message = Devv::DevvMessageUniquePtr(new Devv::DevvMessage);
        message->uri = "my_uri1";
        message->message_type = Devv::eMessageType::VALID;
        LOG(info) << "Sending message";
        t1_server->queueMessage(std::move(message));
      }
    });
    all_threads.emplace_back(std::move(t1_server_thread));
  }

  sleep(2);
  // Setup the T2 client. This will connect to
  // other nodes
  Devv::io::TransactionClient t2_client{context};
  for (auto i : t2_host_vector) {
    t2_client.addConnection(i);
  }
  t2_client.attachCallback(print_t2_devv_message);

  std::thread t2_client_thread([&t2_client]() noexcept {
    LOG(info) << "Starting T2 client thread ...";
    t2_client.run();
    LOG(info) << "Client stopped.";
  });

  all_threads.emplace_back(std::move(t2_client_thread));

  typedef std::unique_ptr<Devv::io::TransactionServer> TSPtr;
  TSPtr t2_server;
  if (t2_bind_endpoint.size() > 0) {
    t2_server = TSPtr(new Devv::io::TransactionServer(context, t2_bind_endpoint));
    t2_server->startServer();
    std::thread t2_server_thread([&t2_server]() {
      for (;;) {
        sleep(5);
        auto message = Devv::DevvMessageUniquePtr(new Devv::DevvMessage);
        message->uri = "my_uri2";
        message->message_type = Devv::eMessageType::VALID;
        LOG(info) << "Sending message";
        t2_server->queueMessage(std::move(message));
      }
    });
    all_threads.emplace_back(std::move(t2_server_thread));
  }

  for (auto& t : all_threads) {
    t.join();
  }

  return 0;
}
