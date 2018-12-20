/*
 * producer_consumer_queue_test.cpp - tests the producer consumer queue.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#include <memory>
#include "queues/DevvRingQueue.h"
#include "types/DevvMessage.h"

#include "folly/ProducerConsumerQueue.h"

const int kMessageCount = 100000;
using namespace Devv;

std::unique_ptr<DevvMessage> get_unique() {
  std::vector<uint8_t> data(100);
  eMessageType msg{eMessageType::VALID};
  URI uri = "Hello";

  auto ptr = std::unique_ptr<DevvMessage>(new DevvMessage(uri, msg, data));
  return ptr;
}

void
put_unique(std::unique_ptr<DevvMessage> message) {
}

struct BufferTester {
  bool operator()(std::unique_ptr<DevvMessage> message) {
    ++tot_count;
    if (!(tot_count % 1000)) {
      LOG_DEBUG << "Got " << int(tot_count/1000) << " messages";
    }
    if (message->message_type == eMessageType::VALID) {
      ++valid_count;
    }
    if (tot_count >= kMessageCount) {
      return true;
    } else {
      return false;
    }
  }
  int tot_count = 0;
  int valid_count = 0;
};

int
main(int, char**) {

  LOG_DEBUG << "Going to run!!";

  folly::ProducerConsumerQueue<DevvMessage> queue(10);

  BufferTester func;

  auto msg = get_unique();

  std::thread reader([&queue, &func] {
      bool quit = false;
      LOG_DEBUG << "Going to pop!!";
      do {
        while (!queue.read(str)) {
          //spin until we get a value
          continue;
        }
        quit = func(rq.pop());
      } while (!quit);
    });

  for (auto i = 0; i < kMessageCount; i++) {
    if (!(i % 1000)) {
      LOG_DEBUG << "Sent " << int(i/1000) << " messages";
    }
    auto msg = get_unique();
    rq.push(std::move(msg));
  }

  LOG_DEBUG << "Waiting...";
  reader.join();

  LOG_DEBUG << "Done";

  return 0;
};
