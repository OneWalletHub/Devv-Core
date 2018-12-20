/*
 * TransactionCreationManager.h supports concurrency for Devv
 * by providing means to create or prase multiple transactions
 * in parallel.
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef CONCURRENCY_TRANSACTIONCREATIONMANAGER_H_
#define CONCURRENCY_TRANSACTIONCREATIONMANAGER_H_

#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/thread_pool.hpp>
#include <boost/thread.hpp>

#include "primitives/buffers.h"
#include "primitives/factories.h"

namespace Devv
{

typedef boost::packaged_task<bool> task_t;
typedef boost::shared_ptr<task_t> ptask_t;

static inline void push_job(Transaction& x
              , const KeyRing& keyring
              , boost::asio::io_service& io_service
              , std::vector<boost::shared_future<bool>>& pending_data) {
  ptask_t task = boost::make_shared<task_t>([&x, &keyring]() {
      x.setIsSound(keyring);
      return(true);
    });
  boost::shared_future<bool> fut(task->get_future());
  pending_data.push_back(fut);
  io_service.post(boost::bind(&task_t::operator(), task));
}

class TransactionCreationManager {
public:
  TransactionCreationManager(TransactionCreationManager&) = delete;

  TransactionCreationManager(eAppMode mode, int num_threads = -1)
    : io_service_()
    , threads_()
    , work_(io_service_)
    , app_mode_(mode)
    , num_threads_(num_threads)
  {
    if (num_threads_ < 0) {
      num_threads_ = boost::thread::hardware_concurrency();
    }
    LOG_DEBUG << "TransactionCreationManager: setting thread count to " << num_threads_;
    for (int i = 0; i < num_threads_; ++i)
    {
        threads_.create_thread(boost::bind(&boost::asio::io_service::run,
                                          &io_service_));
    }
  }

  /**
   * Create Transactions objects from serialized binary data.
   * @param buffer - buffer containing a binary series of Transactions
   * @param vtx - (out) target vector for pointers to the Transaction objects
   * @param start_size - the starting offset of Transaction bytes in the buffer
   * @param tx_size - the total byte size of this block of serial Transactions
   * @return std::vector<std::vector<byte>> raw transactions - the canonical transactions in series
   */
  std::vector<std::vector<byte>> CreateTransactions(InputBuffer& buffer
                          , std::vector<TransactionPtr>& vtx
                          , size_t start_size
                          , size_t& tx_size) {

    std::vector<std::vector<byte>> raw_txs;
    while (buffer.getOffset() < (start_size + tx_size)) {
      //Transaction constructor increments offset by ref
      LOG_TRACE << "while, offset = " << buffer.getOffset();
      auto tx = CreateTransaction(buffer,
                                  *keys_p_,
                                  app_mode_,
                                  false);
      raw_txs.push_back(tx->getCanonical());
      vtx.push_back(std::move(tx));
    }

    if (num_threads_ > 0) {
      std::vector<boost::shared_future<bool>> pending_data; // vector of futures

      for (auto& tx : vtx) {
        push_job(*tx, *keys_p_, io_service_, pending_data);
      }

      boost::wait_for_all(pending_data.begin(), pending_data.end());
    } else {
      for (auto& tx : vtx) {
        tx->isSound(*keys_p_);
      }

    }
    return raw_txs;
  }

  /**
   * Set a pointer to the directory of keys and address.
   * @param keys - a pointer to the directory of keys and addresses.
   */
  void set_keys(const KeyRing* keys) {
    keys_p_ = keys;
  }

 private:
  const KeyRing* keys_p_ = nullptr;
  boost::asio::io_service io_service_;
  boost::thread_group threads_;
  boost::asio::io_service::work work_;
  const eAppMode app_mode_;
  int num_threads_;
};

}

#endif /* CONCURRENCY_TRANSACTIONCREATIONMANAGER_H_ */
