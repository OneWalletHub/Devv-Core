/*
 * primitives/binary_tools.h defines tools for checking if binaries contain Devv primitives
 *
 * @copywrite  2018 Devvio Inc
 */
#pragma once

#include "primitives/FinalBlock.h"

namespace Devv {

/** Checks if binary is encoding a block
 * @note this function is pretty heuristic, do not use in critical cases
 * @return true if this data encodes a block
 * @return false otherwise
 */
bool IsBlockData(const std::vector<byte>& raw);

/** Checks if binary is encoding Transactions
 * @note this function is pretty heuristic, do not use in critical cases
 * @return true if this data encodes Transactions
 * @return false otherwise
 */
bool IsTxData(const std::vector<byte>& raw);

} // namespace Devv