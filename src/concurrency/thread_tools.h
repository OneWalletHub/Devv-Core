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

/**
 * Abstract base class and interface for locking classes.
 * Different locking class implementations contain different behaviors.
 * In most cases, the UniqueLock implementation will be used for threaded
 * applications and the NoOpLock will be used for single-threaded apps.
 */
class ILock {
 public:
  /**
   * Default constructor
   */
  ILock() {}

  /**
   * Default virtual destructor
   */
  virtual ~ILock() {};

  /**
   * Deleted copy constructor
   */
  ILock(const ILock&) = delete;

  /**
   * Move constructor
   * @param lk object to move from
   */
  ILock(ILock&& lk) = default;

  /**
   * Lock this object
   */
  virtual void lock() = 0;

  /**
   * Attempt to lock the object, but do not block.
   * @return true iff the lock was acquired
   */
  virtual bool try_lock() = 0;

  /**
   * Check if the current thread owns the lock
   * @return true iff this thread owns the lock
   */
  virtual bool is_locked() const = 0;

  /**
   * Unlock/release
   */
  virtual void unlock() = 0;

  /**
   * Create a clone of this lock. Cloning creates a duplicate object
   * that internally references the same locking mechanism. A cloned Lock will
   * share the same locking resource as the original
   * @return a unique_ptr to a cloned lock
   */
  virtual std::unique_ptr<ILock> clone() const = 0;

  /**
   * Serialize this lock into the ostream. This is a helper function
   * for quickly printing the state of the lock
   * @param os the stream to write to
   */
  virtual void serialize(std::ostream& os) const = 0;
};

/// Stream operator
inline std::ostream& operator<<(std::ostream& os, const ILock& lk) {
  lk.serialize(os);
  return os;
}

/**
 * A NoOpLock is primarily used to run in a single-threaded environment,
 * in particular for testing. The NoOpLock does not perform any locking; it
 * simply maintains a counter that tracks the number of locks and unlocks
 * that have occurred.
 */
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
    os << "NoOp::getLockCount(" << getLockCount() << ")";
  }

 private:
  std::atomic<int32_t> lock_count_ = ATOMIC_VAR_INIT(0);
};

inline std::ostream& operator<<(std::ostream& os, const NoOpLock& lk) {
  lk.serialize(os);
  return os;
}

/**
 * The UniqueLock provides a mechanism to acquire a lock and hold it until it
 * the object goes out of scope or the mutex is explicitly unlocked. When multiple
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
    mutex_shared_ptr_ = lk.mutex_shared_ptr_;
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
