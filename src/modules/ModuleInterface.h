/*
 * ModuleInterface
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

namespace Devv {

class ModuleInterface {
 public:
  ModuleInterface() {}

  virtual ~ModuleInterface() {}

  /**
   * Initialize the module
   */
  virtual void init() = 0;

  /**
   * Basic tests to ensure the module has been initialized properly
   */
  virtual void performSanityChecks() = 0;

  /**
   * Shut this module down
   */
  virtual void shutdown() = 0;

  /**
   * Starts the module thread(s) if enabled
   */
  virtual void start() = 0;
};

} // namespace Devv