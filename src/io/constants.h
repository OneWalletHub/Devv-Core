/*
 * constants.h tests
 * tracks constants for the Devv networking and IO module.
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <chrono>
#include <string>

namespace Devv {
namespace io {

namespace constants {
  // the default I/O read timeout in milliseconds
  static constexpr std::chrono::milliseconds kReadTimeout{500};
}

} // namespace io
} // namespace Devv
