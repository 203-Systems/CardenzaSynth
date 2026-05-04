#include "OS/UI/UI.h"

#include <sstream>

namespace ui {

ParamMatrix MakeEmptyMatrix() {
  ParamMatrix matrix{};
  for (auto& cell : matrix)
  {
    cell = ParamCell{"--", ""};
  }
  return matrix;
}

std::string RenderParamMatrix(const ParamMatrix& matrix, std::size_t columns) {
  std::ostringstream stream;
  for (std::size_t index = 0; index < matrix.size(); ++index)
  {
    if (index > 0)
    {
      if (index % columns == 0)
      {
        stream << "\n";
      }
      else
      {
        stream << ' ';
      }
    }

    stream << '[' << matrix[index].label << ']';
  }
  return stream.str();
}

}  // namespace ui
