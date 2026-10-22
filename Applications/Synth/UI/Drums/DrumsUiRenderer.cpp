#include "Applications/Synth/UI/Drums/DrumsUiRenderer.h"

#include <algorithm>

#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

namespace synth::ui_render {

namespace {
// Drum-machine shorthand keeps one label per performance column; full names appear on a hit.
constexpr std::array<const char*, 8> kPadCodes{{"BD", "SD", "CH", "OH", "CP", "TM", "CB", "MA"}};
constexpr std::array<const char*, 8> kPadNames{{"KICK", "SNARE", "C HAT", "O HAT", "CLAP", "TOM", "BELL", "MARACA"}};
}  // namespace

std::string DrawDrumsPads(ui::Canvas& canvas,
                          int x,
                          int y,
                          int height,
                          const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                          float alpha)
{
  std::string hit;
  if (alpha <= 0.01f || height <= 0) return hit;

  const ui::Color pad = FadeColor(kSurface, alpha);
  for (std::size_t column = 0; column < kPadCodes.size(); ++column)
  {
    // Every matrix row repeats the same eight drums, so any row lights the column.
    bool active = false;
    for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
      active = active || activeKeys[row * kKeyboardMatrixColumns + kHomePerformanceFirstColumn + column];
    if (active) hit += (hit.empty() ? "" : " ") + std::string(kPadNames[column]);

    const ui::Rect cell = MakeRect(x + static_cast<int>(column) * (kKeyColumnWidth + kKeyColumnGap), y, kKeyColumnWidth, height);
    const auto key = MakeRect(cell.x, cell.y, cell.width, cell.height - InkHeight(1) - 4);
    canvas.FillRect(key, active ? FadeColor(kAccent, alpha) : pad);
    DrawFrame(canvas, key, FadeColor(active ? kAccent : kRule, alpha));
    const std::string code = kPadCodes[column];
    DrawInk(canvas, cell.x + (cell.width - InkWidth(canvas, code, 1) + 1) / 2, cell.y + cell.height - InkHeight(1), code,
            FadeColor(active ? kAccent : kText, alpha), 1);
  }
  return hit;
}

}  // namespace synth::ui_render
