/*
 * chainstate.cpp holds the state of the chain for each validating fork
 *
 * @copywrite  2018 Devvio Inc
 */

#include "chainstate.h"

#include <stdint.h>
#include <map>
#include <mutex>

#include "primitives/Summary.h"

namespace Devv
{

using namespace Devv;

bool ChainState::addCoin(const SmartCoin& coin) {
  bool no_error = true;
  auto it = state_map_.find(coin.getAddress());
  if (it != state_map_.end()) {
    it->second[coin.getCoin()] += coin.getAmount();
  } else {
    CoinMap inner;
    inner.insert(std::make_pair(coin.getCoin(), coin.getAmount()));
    std::pair<Address, CoinMap> outer(coin.getAddress(), inner);
    auto result = state_map_.insert(outer);
    no_error = result.second && no_error;
  }
  LOG_DEBUG << "ChainState::AddCoin(): addr(" <<
            coin.getAddress().getHexString() << ") coin(" << coin.getCoin() << ") amount("<<coin.getAmount()<<") result("<<no_error<<")";
  return(no_error);
}

int64_t ChainState::getAmount(uint64_t type, const Address& addr) const {
  auto it = state_map_.find(addr);
  if (it != state_map_.end()) {
    int64_t amount = it->second.at(type);
    return amount;
  }
  return(0);
}

} //end namespace Devv
