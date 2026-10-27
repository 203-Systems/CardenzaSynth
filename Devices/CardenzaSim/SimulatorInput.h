#pragma once

#include <cstddef>

namespace CardenzaSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed);
void SetHomeButton(bool pressed);
void ClearMatrixKeys();
// Simulates taking the SD card out (false) and putting it back (true, the default).
void SetCardInserted(bool inserted);

}  // namespace CardenzaSim::Input
