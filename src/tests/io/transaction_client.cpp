#include <vector>
#include <string>

#include <boost/program_options.hpp>

#include "io/message_service.h"
#include "io/constants.h"

void print_devcash_message(Devcash::DevcashMessageUniquePtr message) {
  LOG(info) << "Got a message";
  LOG(info) << "DC->uri: " << message->uri;
  return;
}


namespace po = boost::program_options;

int
main(int argc, char** argv) {

  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " uri" << std::endl;
    std::cerr << "ex: " << argv[0] << " tcp://localhost:55557" << std::endl;
    exit(-1);
  }

  std::string host_uri = argv[1];

  std::cout << "Connecting to " << host_uri << std::endl;

  /*

  int opt;
  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "produce help message")
    ("optimization", po::value<int>(&opt)->default_value(10),
     "optimization level")
    ("include-path,I", po::value< std::vector<std::string> >(),
     "include path")
    ;

     std::vector<std::thread> allThreads{};
  */
  // Zmq Context
  zmq::context_t context(1);

  // start ZmqClient
  Devcash::io::TransactionClient client{context};
  client.AddConnection(host_uri);
  client.AttachCallback(print_devcash_message);

  client.Run();

  /*
  std::thread clientThread([&client]() noexcept {
    LOG(info) << "Starting Client thread ...";
    client.Run();
    LOG(info) << "Client stopped.";
  });
  */

  /*
  client.waitUntilRunning();
  allThreads.emplace_back(std::move(clientThread));

  LOG(info) << "Starting main event loop...";
  mainEventLoop.run();
  LOG(info) << "Main event loop got stopped";

  client.stop();
  client.waitUntilStopped();
  */

  /*
  for (auto& t : allThreads) {
    t.join();
  }
  */
  return 0;
}
