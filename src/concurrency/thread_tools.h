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

/**
 * A stream operator used to serialize an ILock object. The stream operator calls
 * the ILock::serialize() member function which will resolve to the serialize() member
 * function of the derived object
 *
 * @param os the stream in which to serialize to
 * @param lk the lock to serialize from
 * @return the same input stream after the serialization
 */
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
  /**
   * Default constructor
   */
  NoOpLock() = default;

  /**
   * Default destructor
   */
  virtual ~NoOpLock() = default;

  NoOpLock(const NoOpLock&) = delete;

  /**
   * Increment the lock_count
   */
  void lock() override {
    ++lock_count_;
  }

  /**
   * Increment the lock_count
   * @return true
   */
  bool try_lock() override {
    ++lock_count_;
    return true;
  }

  /**
   * Returns false
   * @return false
   */
  bool is_locked() const override {
    return false;
  }

  /**
   * Decrements the lock_count
   */
  void unlock() override {
    --lock_count_;
  }

  /**
   * Clone this lock. Copies the lock count to the
   * cloned lock, but subsequently the locks are
   * independent.
   * @return a new unique_ptr to an ILock
   */
  std::unique_ptr<ILock> clone() const override {
    auto lk = std::make_unique<NoOpLock>();
    lk->lock_count_ = lock_count_.load();
    return lk;
  }

  /**
   * Get the current lock count
   * @return
   */
  int32_t getLockCount() const {
    return lock_count_;
  }

  /**
   * Serialize the lock_count into the output stream
   * @param os output stream to stream into
   */
  void serialize(std::ostream& os) const override {
    os << "NoOp::getLockCount(" << getLockCount() << ")";
  }

 private:
  /// Tracks the lock/unlock calls
  std::atomic<int32_t> lock_count_ = ATOMIC_VAR_INIT(0);
};

/**
 * A stream operator used to serialize a NoOpLock object
 *
 * @param os the stream in which to serialize to
 * @param lk the lock to serialize from
 * @return the same input stream after the serialization
 */
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
  /**
   * Default constructor. Initializes lock with new mutex_shared_ptr
   * and creates an unlocked unique_lock
   */
  UniqueLock()
      : mutex_shared_ptr_(std::make_shared<std::mutex>())
      , lock_ptr_(std::make_unique<std::unique_lock<std::mutex>>(*mutex_shared_ptr_, std::defer_lock)) {
    LOG_DEBUG << "UniqueLock(): " << *this;
  }

  /**
   * Creates a UniqueLock that shares a shared_ptr mutex.
   * @param mutex
   */
  UniqueLock(std::shared_ptr<std::mutex> mutex)
      : mutex_shared_ptr_(mutex)
      , lock_ptr_(std::make_unique<std::unique_lock<std::mutex>>(*mutex_shared_ptr_, std::defer_lock)) {
    LOG_DEBUG << "UniqueLock(): " << *this;
  }

  /**
   * Destructor
   */
  ~UniqueLock() {
    LOG_DEBUG << "~UniqueLock(): UNLOCK";
  }

  /**
   * Deleted copy constructor
   */
  UniqueLock(const UniqueLock&) = delete;

  /**
   * Move constructor
   * @param lk UniqueLock to move from
   */
  UniqueLock(UniqueLock&& lk) : ILock(std::move(lk)) {
    lock_ptr_ = std::move(lk.lock_ptr_);
    mutex_shared_ptr_ = lk.mutex_shared_ptr_;
  }

  /**
   * Lock the underlying resource (mutex)
   */
  void lock() {
    LOG_DEBUG << "UniqueLock::lock()ing " << *this;
    lock_ptr_->lock();
    LOG_DEBUG << "UniqueLock::lock()ed " << *this;
  }

  /**
   * Attempt to lock the resource. This is a non-blocking call that returns immediately.
   * @return true iff the lock was acquired successfully by this thread
   */
  bool try_lock() {
    lock_ptr_->try_lock();
    LOG_DEBUG << "UniqueLock::try_lock() " << *this;
    return lock_ptr_->owns_lock();
  }

  /**
   * Check if the underlying resources is locked and owned by this thread.
   * @return true iff this thread owns the lock
   */
  bool is_locked() const {
    return lock_ptr_->owns_lock();
  }

  /**
   * Unlock this resource. Calls member unlock of the managed mutex object, and sets the owning
   * state to false. If the owning state is false before the call, the function throws a system_error
   * exception with operation_not_permitted as error condition.
   */
  void unlock() {
    lock_ptr_->unlock();
  }

  /**
   * Creates a new UniqueLock that shares the same underlying resources as this
   * lock object.
   *
   * @return a unique_ptr to an ILock object (abstract base class)
   */
  std::unique_ptr<ILock> clone() const {
    return std::make_unique<UniqueLock>(mutex_shared_ptr_);
  }

  /**
   * Streams either "LOCK" or "UNLOCK" to the stream corresponding to the state
   * of the internal mutex.
   *
   * @param os the output stream to serialize into
   */
  void serialize(std::ostream& os) const override {
    if (is_locked()) {
      os << "  LOCK:";
    } else {
      os << "UNLOCK:";
    }
  }

 private:
  /// Underlying resource used to manage the state of this lock
  std::shared_ptr<std::mutex> mutex_shared_ptr_;

  /// The unique_lock responsible for locking this resource (mutex)
  std::unique_ptr<std::unique_lock<std::mutex>> lock_ptr_;
};

/**
 * A stream operator used to serialize a UniqueLock object
 *
 * @param os the stream in which to serialize to
 * @param lk the lock to serialize from
 * @return the same input stream after the serialization
 */
inline std::ostream& operator<<(std::ostream& os, const UniqueLock& lk) {
  lk.serialize(os);
  return os;
}

} // namespace Devv
