/*
 * block_tools.cpp implements tools for
 * checking if binary data contains Devv primitives
 *
 * @copywrite  2018 Devvio Inc
 */
#include "binary_tools.h"

#include <sstream>
#include <iomanip>

namespace Devv {

bool IsBlockData(const std::vector<byte>& raw) {
  //check if big enough
  if (raw.size() < FinalBlock::MinSize()) { return false; }
  //check version
  if (raw[0] != 0x00) { return false; }
  size_t offset = 9;
  uint64_t block_time = BinToUint64(raw, offset);
  // check blocktime is from 2018 or newer.
  if (block_time < 1514764800) { return false; }
  // check blocktime is in past
  if (block_time > GetMillisecondsSinceEpoch()) { return false; }
  return true;
}

bool IsTxData(const std::vector<byte>& raw) {
  // check if big enough
  if (raw.size() < Transaction::MinSize()) {
    LOG_WARNING << "raw.size()(" << raw.size() << ") < Transaction::MinSize()(" << Transaction::MinSize() << ")";
    return false;
  }
  // check transfer count
  uint64_t xfer_size = BinToUint64(raw, 0);
  size_t tx_size = Transaction::MinSize() + xfer_size;
  if (raw.size() < tx_size) {
    LOG_WARNING << "raw.size()(" << raw.size() << ") < tx_size(" << tx_size << ")";
    return false;
  }
  // check operation
  if (raw[16] >= 4) {
    LOG_WARNING << "raw[8](" << int(raw[16]) << ") >= 4";
    return false;
  }
  return true;
}

} // namespace Devv