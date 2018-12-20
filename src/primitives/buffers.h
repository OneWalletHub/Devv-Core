/*
 * buffers.h provides methods to track and utilize Devv binary data
 *
 * @copywrite  2018 Devvio Inc
 */
#ifndef DEVV_BUFFERS_H
#define DEVV_BUFFERS_H

#include <vector>
#include "common/binary_converters.h"
#include "common/devv_constants.h"
#include "common/devv_types.h"
#include "primitives/Address.h"
#include "primitives/Signature.h"

namespace Devv {

/**
 * Class used to help deserialize input buffers.
 * InputBuffer contains a reference to a vector<byte> and
 * a size_t offet that indicates the current position. Upon
 * initialization, the input buffer is a const reference, so
 * the user must ensure the input vector<byte> remains valid
 * for the lifetime of the InputBuffer. Since the buffer is
 * const and the offset gets updated, InputBuffer should always
 * be passed as a non-const reference.
 */
class InputBuffer {
 public:
  /**
   * Delete default constructor
   */
  InputBuffer() = delete;

  /**
   * Delete copy constructor
   * @param buffer
   */
  InputBuffer(const InputBuffer& buffer) = delete;

  /**
   * Delete assignment operator
   * @return
   */
  InputBuffer& operator=(const InputBuffer&) = delete;

  /**
   * Constructor that initializes this object with the given input buffer and
   * offset. The input_buffer must remain valid and in scope for the lifetime
   * of this object.
   *
   * @param input_buffer The input buffer reference to deserialize
   * @param offset The current offset location
   */
  explicit InputBuffer(const std::vector<byte>& input_buffer, size_t offset = 0)
      : buffer_(input_buffer)
      , offset_(offset) {}

  /**
   * Returns the byte at the given position
   *
   * @param loc
   * @return
   */
  byte at(size_t loc) {
    return buffer_.at(loc);
  }

  /**
   * Returns the byte at the given position with the offset
   *
   * @param loc
   * @return
   */
  byte offsetAt(size_t loc) {
    return buffer_.at(loc+offset_);
  }

  /**
   * Copies the size length out of the buffer and
   * increment the position by length
   *
   * @tparam Iter
   * @param out_iter
   * @param length
   */
  template <typename Iter>
  void copy(Iter out_iter, size_t length, bool increment_buffer = true)
  {
    std::copy_n(buffer_.begin() + offset_, length, out_iter);
    if (increment_buffer) { offset_ += length; }
  }

  /**
   * Copies the size N out of the buffer and into the array
   * Increments offset_ by N if increment_buffer is true
   *
   * @tparam N Size of the array
   * @param array
   * @param increment_buffer
   */
  template <size_t N>
  void copy(std::array<byte, N>& array, bool increment_buffer = true)
  {
    std::copy_n(buffer_.begin() + offset_, N, array.begin());
    if (increment_buffer) { offset_ += N; }
  }

  void copy(Address& addr, bool increment_buffer = true)
  {
    size_t t = getNextByte();
    std::vector<byte> bin_addr(buffer_.begin() + offset_, buffer_.begin() + offset_ + t);
    Address new_addr(bin_addr);
    addr = new_addr;
    if (increment_buffer) { offset_ += bin_addr.size(); }
  }

  void copy(Signature& sig, bool increment_buffer = true)
  {
    size_t t = getNextByte();
    std::vector<byte> bin_sig(buffer_.begin() + offset_, buffer_.begin() + offset_ + t);
    Signature new_sig(bin_sig);
    sig = new_sig;
    if (increment_buffer) { offset_ += bin_sig.size(); }
  }

  /**
   * Returns the byte at the current position
   *
   * @param loc
   * @return
   */
  byte getNextByte(bool increment_buffer = true) {
    byte ret = at(offset_);
    if (increment_buffer) { offset_ += sizeof(ret); }
    return ret;
  }

  std::vector<byte> getNextTransaction() {
    std::vector<byte> out;
    if (size() < offset_ + kTX_MIN_SIZE) {
      return getNextT1Transaction();
    }
    /// Don't increment the buffer, we want to copy it all to canonical_
    uint64_t xfer_size = getNextUint64(false);
    uint64_t nonce_size = getSecondUint64(false);

    if (nonce_size < kMIN_NONCE_SIZE) {
      return getNextT1Transaction();
    }

    byte oper = offsetAt(kOPERATION_OFFSET);
    if (oper >= eOpType::NumOperations) {
      return getNextT1Transaction();
    }

    size_t tx_size = offsetAt(kTRANSFER_OFFSET+xfer_size+nonce_size)
                     +kTRANSFER_OFFSET+xfer_size+nonce_size+1;

    if (size() < offset_ + tx_size) {
      LOG_WARNING << "Buffer is smaller than calculated Tier2Transaction size.";
      return getNextT1Transaction();
    }
    copy(std::back_inserter(out), tx_size);
    return out;
  }

  std::vector<byte> getNextT1Transaction() {
    std::vector<byte> out;
    uint64_t sum_size = getNextUint64(false);
    size_t tx_size = sum_size + kNODE_SIG_BUF_SIZE + kUINT64_SIZE + kNODE_ADDR_BUF_SIZE;
    if (size() < offset_ + tx_size) {
      LOG_WARNING << "Invalid serialized transaction, too small!";
      return out;
    }

    copy(std::back_inserter(out), tx_size);
    return out;
  }

  /**
   * Returns the next uint32_t and increment the buffer
   * by the size if increment_buffer is true
   *
   * @param increment_buffer
   * @return
   */
  uint32_t getNextUint32(bool increment_buffer = true)
  {
    uint32_t ret(BinToUint32(buffer_, offset_));
    if (increment_buffer) { offset_ += sizeof(ret); }
    return ret;
  }

  /**
   * Returns the next uint64_t and increment the buffer
   * by the size if increment_buffer is true
   *
   * @param increment_buffer
   * @return
   */
  uint64_t getNextUint64(bool increment_buffer = true)
  {
    uint64_t ret(BinToUint64(buffer_, offset_));
    if (increment_buffer) { offset_ += sizeof(ret); }
    return ret;
  }

  /**
   * Returns the second uint64_t in the buffer and increment the buffer
   * by the size if increment_buffer is true
   *
   * @param increment_buffer
   * @return
   */
  uint64_t getSecondUint64(bool increment_buffer = true)
  {
    uint64_t ret(BinToUint64(buffer_, offset_+8));
    if (increment_buffer) { offset_ += sizeof(ret); }
    return ret;
  }

  /**
   * Return a const ref to the full buffer
   * @return
   */
  const std::vector<byte>& getBuffer() const {
    return buffer_;
  }

  /**
   * Get the iterator at the current offset
   * @return iterator at current offset
   */
  std::vector<byte>::const_iterator getCurrentIterator() {
    return(buffer_.begin() + offset_);
  }

  /**
   * Return the current offset of this buffer
   * @return
   */
  size_t getOffset() const {
    return offset_;
  }

  /**
   * Increment the offset by amount.
   *
   * @param amount
   */
  void increment(size_t amount) {
    offset_ += amount;
  }

  /**
   * Return the size of the buffer.
   * @return
   */
  size_t size() const {
    return buffer_.size();
  }

  void reset() {
    offset_ = 0;
  }

 private:
  /// Serialized buffer
  std::vector<byte> buffer_;
  /// Current offset
  size_t offset_ = 0;
};

} // namespace Devv
#endif //DEVV_BUFFERS_H
