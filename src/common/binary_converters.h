/*
 * binary_converters.h includes functions for common conversions.
 *
 * @copywrite  2018 Devvio Inc
 */
#ifndef DEVV_BINARY_CONVERTERS_H
#define DEVV_BINARY_CONVERTERS_H

#include "common/util.h"

namespace Devv {

static const size_t kBYTES_PER_INT = 4;

static int32_t BinToInt32(const std::vector<byte>& bytes, size_t start) {
  // MTR_SCOPE_FUNC();
  int32_t dest = 0;
  for (unsigned int i = 0; i < kBYTES_PER_INT; ++i) {
    dest |= (bytes.at(start + i) << (i * 8));
  }
  return dest;
}

static std::vector<byte> Int32ToBin(const int32_t& source, std::vector<byte>& dest) {
  // MTR_SCOPE_FUNC();
  for (unsigned int i = 0; i < kBYTES_PER_INT; ++i) {
    dest.push_back((source >> (i * 8)) & 0xFF);
  }
  return dest;
}

static uint32_t BinToUint32(const std::vector<byte>& bytes, size_t start) {
  // MTR_SCOPE_FUNC();
  uint32_t dest = 0;
  for (unsigned int i = 0; i < kBYTES_PER_INT; ++i) {
    dest |= (bytes.at(start + i) << (i * 8));
  }
  return dest;
}

static std::vector<byte> Uint32ToBin(const uint32_t& source, std::vector<byte>& dest) {
  // MTR_SCOPE_FUNC();
  for (unsigned int i = 0; i < kBYTES_PER_INT; ++i) {
    dest.push_back((source >> (i * 8)) & 0xFF);
  }
  return dest;
}

/**
 * Deserialize byte array to unsigned 64-bit integer
 * @param[in] bytes Incoming byte array
 * @param[in] start array location of uint64_t
 * @return deserialized 64-bit integer
 */
static uint64_t BinToUint64(const std::vector<byte>& bytes, size_t start) {
  // MTR_SCOPE_FUNC();
  uint32_t lsb = BinToUint32(bytes, start);
  uint32_t msb = BinToUint32(bytes, start + kBYTES_PER_INT);
  uint64_t dest = (uint64_t(msb) << 32) | uint64_t(lsb);
  return dest;
}

/**
 * Convert 64-bit unsigned int to to binary
 * @param[in] source
 * @param[out] dest
 * @return
 */
static std::vector<byte> Uint64ToBin(const uint64_t& source, std::vector<byte>& dest) {
  // MTR_SCOPE_FUNC();
  uint32_t lsb = source & 0xffffffff;
  uint32_t msb = source >> 32;
  Uint32ToBin(lsb, dest);
  Uint32ToBin(msb, dest);
  return dest;
}

/**
 * Creat 64-bit int from binary stream
 * @param bytes
 * @param start
 * @return
 */
static int64_t BinToInt64(const std::vector<byte>& bytes, size_t start) {
  // MTR_SCOPE_FUNC();
  uint32_t lsb = BinToUint32(bytes, start);
  uint32_t msb = BinToUint32(bytes, start + kBYTES_PER_INT);
  uint64_t d1 = (uint64_t(msb) << 32);
  int64_t dest = d1 | uint64_t(lsb);
  return dest;
}

static std::vector<byte> Int64ToBin(const int64_t& source, std::vector<byte>& dest) {
  // MTR_SCOPE_FUNC();
  uint32_t lsb = source & 0xffffffff;
  uint32_t msb = source >> 32;
  Uint32ToBin(lsb, dest);
  Uint32ToBin(msb, dest);
  return dest;
}

/**
 * Maps a hex digit to an int value.
 * @param hex digit to get the int value for
 * @return int value of this hex digit
 */
static int Char2Int(char in) {
  // MTR_SCOPE_FUNC();
  if (in >= '0' && in <= '9') {
    return (in - '0');
  }
  if (in >= 'A' && in <= 'F') {
    return (in - 'A' + 10);
  }
  if (in >= 'a' && in <= 'f') {
    return (in - 'a' + 10);
  }
  return (-1);
}

/**
 * Maps a string into a byte vector.
 * @param str a string to encode directly into binary
 * @return a byte vector of the same data in binary form.
 */
static std::vector<byte> Str2Bin(const std::string& str) {
  std::vector<byte> out(str.begin(), str.end());
  return out;
}

/**
 * Maps a byte vector into a string.
 * @param bin a binary vector
 * @return a string containing the same data
 */
static std::string Bin2Str(const std::vector<byte>& bin) {
  std::string out(bin.begin(), bin.end());
  return out;
}

/**
 * Maps a hex string into a byte vector.
 * @param hex a string of hex digits.
 * @return a byte vector of the same data in binary form.
 */
static std::vector<byte> Hex2Bin(const std::string& hex) {
  MTR_SCOPE_FUNC();
  int len = hex.length();
  std::vector<uint8_t> buf(len / 2 + 1);
  for (int i = 0; i < len / 2; i++) {
    buf.at(i) = (byte)Char2Int(hex.at(i * 2)) * 16 + Char2Int(hex.at(1 + i * 2));
  }
  buf.pop_back();  // remove null terminator
  return (buf);
}

/** chars for hex conversions */
static const char alpha[] = "0123456789ABCDEF";

/**
 * Change binary data into a hex string.
 * @pre input must be allocated to a length of len
 * @param input pointer to the binary data
 * @param len length of the binary data
 * @return string containing these data as hex numbers
 */
template <typename Array>
static std::string ToHex(const Array& input, size_t num_bytes = UINT32_MAX) {
  MTR_SCOPE_FUNC();
  std::stringstream ss;
  for (size_t j = 0; j < std::min(num_bytes, input.size()); j++) {
    int c = (int)input.at(j);
    ss.put(alpha[(c >> 4) & 0xF]);
    ss.put(alpha[c & 0xF]);
  }
  return (ss.str());
}

}  // namespace Devv
#endif  // DEVV_BINARY_CONVERTERS_H
