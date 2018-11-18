/*
 * chainstate.h holds the state of the chain for each validating fork
 *
 * @copywrite  2018 Devvio Inc
 */

#ifndef SRC_CONSENSUS_STATESTUB_H_
#define SRC_CONSENSUS_STATESTUB_H_

#include <map>
#include <mutex>

#include "primitives/SmartCoin.h"

namespace Devv
{

class ChainState {
public:

/** Constructor */
  ChainState() noexcept = default;

  /**
   * Default copy constructor
   *
   * @param other
   */
  ChainState(const ChainState& other) = default;

  /**
   * Move constructor
   * @param other
   */
  ChainState(ChainState&& other) noexcept = default;

  static ChainState Copy(const ChainState& state);

  ChainState& operator=(const ChainState&& other)
  {
    if (this != &other) {
      this->state_map_ = other.state_map_;
    }
    return *this;
  }

  ChainState& operator=(const ChainState& other)
  {
    if (this != &other) {
      this->state_map_ = other.state_map_;
    }
    return *this;
  }

/** Adds a coin to the state.
 *  @param reference to the coin to add
 *  @return true if the coin was added successfully
 *  @return false otherwise
*/
  bool addCoin(const SmartCoin& coin);

/** Gets the number of coins at a particular location.
 *  @param type the coin type to check
 *  @param the address to check
 *  @return the number of this type of coins at this address
*/
  int64_t getAmount(uint64_t type, const Address& addr) const;

/** Get the map describing this chain state
 *  @return the map describing this chain state
*/
  const std::map<Address, std::map<uint64_t, int64_t>>& getStateMap() const {
    return state_map_;
  }

  std::map<Address, std::map<uint64_t, int64_t>>& getStateMap() {
    return state_map_;
  }

  size_t size() const {
    return state_map_.size();
  }

private:
 std::map<Address, std::map<uint64_t, int64_t>> state_map_;

};

inline ChainState ChainState::Copy(const ChainState& state) {
  ChainState new_state(state);
  return new_state;
}

} //namespace Devv

#endif /* SRC_CONSENSUS_STATESTUB_H_ */
