/*
 * devv_exceptions.h
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include <stdexcept>
#include <exception>

namespace Devv {

/**
 * An error occured while handling an incoming DevvMessage
 * Usually these errors occur due to malformed input and are
 * recoverable.
 */
struct DevvMessageError : public std::runtime_error {

  /// Default constructor
  DevvMessageError() = default;

  /**
   * Constructor
   * @param message human-readable error message
   * @return
   */
  explicit DevvMessageError(const std::string& message)
      : std::runtime_error(message)
  {}

  /// Default destructor
  ~DevvMessageError() override = default;
};

/**
 * Indicates an error deserializing an incoming message
 */
struct DeserializationError : public DevvMessageError {
  /**
   * Constructor
   * @param message human-readable error message
   */
  explicit DeserializationError(const std::string& message)
      : DevvMessageError(message)
  {}

  /**
   * Destructor.
   */
  ~DeserializationError() override = default;
};

} // namespace Devv
