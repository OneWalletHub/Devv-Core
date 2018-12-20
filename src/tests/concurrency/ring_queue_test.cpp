#include <memory>
#include <functional>
#include <thread>
#include <condition_variable>
#include <mutex>
#include "concurrency/DevvRingQueue.h"
#include "types/DevvMessage.h"

const int kMessageCount = 100000;
const int kWorkerNum = 10;
using namespace Devv;
DevvRingQueue rq;

class DataStore {
 public:
  DataStore(int tot, int valid) : tot_count(tot), valid_count(valid) {}
  DataStore(const DataStore&) {};
  void incTotal() {
	std::lock_guard<std::mutex> lock(totLock_);
    tot_count++;
  }
  void incValid() {
  	std::lock_guard<std::mutex> lock(validLock_);
    valid_count++;
  }
  int getTotal() {return tot_count;}
  int getValid() {return valid_count;}
  private:
   int tot_count = 0;
   int valid_count = 0;
   std::mutex totLock_;
   std::mutex validLock_;
};

class BufferTester {
 public:
  BufferTester(DataStore* someData) : theData(someData){};
  bool theFunk() {
	while (!stop) {
	  LOG_DEBUG << "Going to pop!!\n";
      DevvMessage* message = rq.pop().get();
      theData->incTotal();
      LOG_DEBUG << "Got " << std::to_string(theData->getTotal()) << " messages\n";
      if (message->message_type == eMessageType::VALID) {
        theData->incValid();
      }
    }
    LOG_DEBUG << "Done working!!\n";
    if (theData->getTotal() >= kMessageCount) {
      return true;
    } else {
      return false;
    }
  }
  DataStore* theData;
  bool stop = false;
};

int main(int, char**) {

  LOG_DEBUG << "Going to run!!\n";
  std::vector<uint8_t> data(100);
  std::vector<BufferTester> workers;
  DataStore* theData = new DataStore(0, 0);

  for (auto i=0; i<kWorkerNum; i++) {
	BufferTester worker(theData);
	std::function<bool()> callForward = std::bind(&BufferTester::theFunk, worker);
	workers.push_back(worker);
    new std::thread(callForward);
  }

  for (auto i = 0; i < kMessageCount; i++) {
    LOG_DEBUG << "Sent " << std::to_string(i) << " messages\n";
    auto ptr = std::unique_ptr<DevvMessage>(new DevvMessage("Hello", eMessageType::VALID, data, 10));
    rq.push(std::move(ptr));
    if (i == kMessageCount-kWorkerNum) { //ensure the workers get a chance to stop
      for (auto iter = workers.begin(); iter != workers.end(); ++iter) {
        iter->stop = true;
	  }
	}
  }

  std::this_thread::sleep_for(std::chrono::seconds(1));  //last chance for workers to stop

  LOG_DEBUG << "Done\n";

  return 0;
}
