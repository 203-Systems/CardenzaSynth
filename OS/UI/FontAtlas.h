#pragma once

#include <cstdint>

namespace ui {

struct RasterGlyph {
  std::uint32_t bitmapOffset = 0;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::int16_t xOffset = 0;
  std::int16_t yOffset = 0;
  std::int16_t xAdvance = 0;
};

struct RasterFont {
  std::uint8_t firstChar = 32;
  std::uint8_t lastChar = 126;
  std::uint16_t lineHeight = 0;
  const std::uint8_t* bitmap = nullptr;
  const RasterGlyph* glyphs = nullptr;
};

const RasterFont& UiFontForScale(int scale);
// The 5x7 tracker glyphs at 1x: six-pixel advance for dense terminal text.
const RasterFont& UiMicroFont();

}  // namespace ui
