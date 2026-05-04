#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace ui {

struct ParamCell {
  std::string label;
  std::string parameterId;
};

using ParamMatrix = std::array<ParamCell, 12>;

ParamMatrix MakeEmptyMatrix();
std::string RenderParamMatrix(const ParamMatrix& matrix, std::size_t columns = 4);

}  // namespace ui
