#pragma once

#include <cstddef>

namespace ComputeCardSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed);
void ClearMatrixKeys();

}  // namespace ComputeCardSim::Input
