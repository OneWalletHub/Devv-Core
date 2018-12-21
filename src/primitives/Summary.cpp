/*
 * Summary.cpp
 *
 * @copywrite  2018 Devvio Inc
 */
#include "Summary.h"
#include "primitives/json_interface.h"

namespace Devv {

bool AddToDelayedMap(uint64_t coin, const DelayedItem& item, DelayedMap& existing) {
  if (existing.count(coin) > 0) {
    DelayedItem the_item = existing.at(coin);
    the_item.delta += item.delta;
    the_item.delay = std::max(the_item.delay, item.delay);
    existing.at(coin) = the_item;
  } else {
    std::pair<uint64_t, DelayedItem> one_item(coin, item);
    existing.insert(one_item);
  }
  return true;
}

bool RemoveFromDelayedMap(uint64_t coin, int64_t delta, DelayedMap& existing) {
  if (existing.count(coin) > 0) {
    DelayedItem the_item = existing.at(coin);
    the_item.delta += delta;
    existing.at(coin) = the_item;
    if (the_item.delta < 0) {
      LOG_WARNING << "Reverted more coins than were pending!";
      return false;
    }
  } else {
    return false;
  }
  return true;
}

bool Summary::isSane() const {
  if (summary_.empty()) { return false; }
  uint64_t coin_total = 0;
  for (auto summary : summary_) {
    auto summary_pair = summary.second;
    auto delayed_map = summary_pair.first;
    auto coin_map = summary_pair.second;
    for (auto delayed_coin : delayed_map) {
      coin_total += delayed_coin.second.delta;
    }
    for (auto coin : coin_map) {
      coin_total += coin.second;
    }
  }
  if (coin_total != 0) {
    LOG_WARNING << "Summary state invalid: " + GetJSON(*this);
    return false;
  }
  return true;
}

}
