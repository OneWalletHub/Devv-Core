/*
 * thread_tools.h
 *
 * Contains resources to assist with threading, concurrency and resource
 * management.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <mutex>
#include <atomic>
#include <ostream>

#include "common/logger.h"

namespace Devv {

class ILock {
 public:
  ILock() {};

  virtual ~ILock() {};

  ILock(const ILock&) = delete;

  ILock(ILock&& lk) {}

  virtual void lock() = 0;

  virtual bool try_lock() = 0;

  virtual bool is_locked() const = 0;

  virtual void unlock() = 0;

  virtual std::unique_ptr<ILock> clone() const = 0;

  virtual void serialize(std::ostream& os) const = 0;
};

inline std::ostream& operator<<(std::ostream& os, const ILock& lk) {
  lk.serialize(os);
  return os;
}

class NoOpLock : public ILock {
 public:
  NoOpLock() {}

  NoOpLock(std::mutex&, bool lock_on_acquire = true) {
    if (lock_on_acquire) ++lock_count_;
  }

  virtual ~NoOpLock() {}

  NoOpLock(const NoOpLock&) = delete;

  void lock() override {
    ++lock_count_;
  }

  bool try_lock() override {
    ++lock_count_;
    return true;
  }

  bool is_locked() const override {
    return false;
  }

  void unlock() override {
    --lock_count_;
  }

  std::unique_ptr<ILock> clone() const override {
    return std::make_unique<NoOpLock>();
  }

  size_t getLockCount() const {
    return lock_count_;
  }

  void serialize(std::ostream& os) const override {
    os << "  NoOp::getLockCount(" << getLockCount() << ")";
  }

 private:
  std::atomic<size_t> lock_count_ = ATOMIC_VAR_INIT(0);
};

inline std::ostream& operator<<(std::ostream& os, const NoOpLock& lk) {
  lk.serialize(os);
  return os;
}

/**
 * The UniqueLock provides a mechanism to acquire a lock and hold it until it
 * the object goes out of scope or the mutex is explicitly locked. When multiple
 * threads share access to data or resources, each thread should instantiate
 * a UniqueLock which shares a mutex to safely access the shared resource.
 */
class UniqueLock : public ILock {
 public:
  UniqueLock()
      : mutex_shared_ptr_(std::make_shared<std::mutex>())
      , lock_ptr_(std::make_unique<std::unique_lock<std::mutex>>(*mutex_shared_ptr_, std::defer_lock)) {
    LOG_DEBUG << "UniqueLock(): " << *this;
  }

  UniqueLock(std::shared_ptr<std::mutex> mutex)
      : mutex_shared_ptr_(mutex)
      , lock_ptr_(std::make_unique<std::unique_lock<std::mutex>>(*mutex_shared_ptr_, std::defer_lock)) {
    LOG_DEBUG << "UniqueLock(): " << *this;
  }

  ~UniqueLock() {
    LOG_DEBUG << "~UniqueLock(): UNLOCK";
  }

  UniqueLock(const UniqueLock&) = delete;

  UniqueLock(UniqueLock&& lk) : ILock(std::move(lk)) {
    lock_ptr_ = std::move(lk.lock_ptr_);
  }

  void lock() {
    LOG_DEBUG << "UniqueLock::lock()ing " << *this;
    lock_ptr_->lock();
    LOG_DEBUG << "UniqueLock::lock()ed " << *this;
  }

  bool try_lock() {
    lock_ptr_->try_lock();
    LOG_DEBUG << "UniqueLock::try_lock() " << *this;
    return lock_ptr_->owns_lock();
  }

  bool is_locked() const {
    return lock_ptr_->owns_lock();
  }

  void unlock() {
    lock_ptr_->unlock();
  }

  std::unique_ptr<ILock> clone() const {
    return std::make_unique<UniqueLock>(mutex_shared_ptr_);
  }

  void serialize(std::ostream& os) const override {
    if (is_locked()) {
      os << "  LOCK:";
    } else {
      os << "UNLOCK:";
    }
  }

 private:
  std::shared_ptr<std::mutex> mutex_shared_ptr_;
  std::unique_ptr<std::unique_lock<std::mutex>> lock_ptr_;
};

inline std::ostream& operator<<(std::ostream& os, const UniqueLock& lk) {
  lk.serialize(os);
  return os;
}

} // namespace Devv
