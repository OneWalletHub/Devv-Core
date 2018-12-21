/*
 * devv_types.h typedefs and enums for Devv.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <cstdint>
#include <cstdlib>
#include <array>

namespace Devv {

typedef unsigned char byte;

// from ossl's sha.h SHA256_DIGEST_LENGTH
const size_t kHASH_LENGTH = 32;

typedef std::array<byte, kHASH_LENGTH> Hash;

/**
 * Tier1 or Tier2 mode
 */
enum eAppMode {
  /// Tier1 mode
  T1 = 0,
  /// Tier2 mode
  T2 = 1,
  /// Scanning mode
  scan = 2
};

} // namespace Devv
