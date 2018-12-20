/*
 * Devv_spsc_queue_test.cpp - tests Devv implementation of the single producer
 * single consumer queue.
 *
 *
 * @copywrite  2018 Devvio Inc
 */

#include <memory>
#include "concurrency/DevvSPSCQueue.h"
#include "types/DevvMessage.h"

const int kMessageCount = 1000000;
const int kMessageSize = 1000;
const int kDebugOutputThrottle = 100000;

using namespace Devv;

static int cnt = 0;

std::unique_ptr<DevvMessage> get_unique() {
  std::vector<uint8_t> data(kMessageSize);
  eMessageType msg{eMessageType::VALID};
  URI uri = "Hello: " + std::to_string(cnt);
  cnt++;


  auto ptr = std::unique_ptr<DevvMessage>(new DevvMessage(uri, msg, data, cnt));
  //LOG_DEBUG << "get_unique: " << ptr.get() << " " << ptr->uri;
  return ptr;
}

void
put_unique(std::unique_ptr<DevvMessage>) {
}

struct BufferTester {
  bool operator()(std::unique_ptr<DevvMessage> message) {
    ++tot_count;
    if (!(tot_count % kDebugOutputThrottle)) {
      LOG_DEBUG << "Got " << tot_count << " messages (" << message->index << ")";
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

  LOG_DEBUG << "Going to create " << kMessageCount*(kMessageSize/1000000.0) << " MB of messages";

  Devv::DevvSPSCQueue rq{};
  BufferTester func;

  //auto msg = get_unique();

  //LOG_DEBUG << "anotheone: " << msg.get();

  std::thread reader([&rq, &func] {
      bool quit = false;
      LOG_DEBUG << "Going to pop!!";
      do {
        quit = func(std::move(rq.pop()));
      } while (!quit);
      LOG_DEBUG << "Done working!!";
    });

  std::vector<std::unique_ptr<DevvMessage>> vec;

  for (auto i = 0; i < kMessageCount; i++) {
    vec.push_back( get_unique());
  }

  auto start = std::chrono::high_resolution_clock::now();
  for (auto i = 0; i < kMessageCount; i++) {
    if (!(i % kDebugOutputThrottle)) {
      LOG_DEBUG << "Sent " << i << " messages";
    }
    //auto msg = get_unique();
    rq.push(std::move(vec.back()));
    vec.pop_back();
  }

  LOG_DEBUG << "Waiting...";
  reader.join();

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double, std::milli> elapsed = end-start;

  LOG_DEBUG << "Waited " << elapsed.count() << " ms";
  LOG_DEBUG << "Done";

  return 0;
}
