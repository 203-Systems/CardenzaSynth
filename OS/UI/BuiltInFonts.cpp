#include "OS/UI/FontAtlas.h"
#include "OS/UI/TrackerFont5x7.h"

#include <array>
#include <cstddef>

namespace ui {
namespace {

// Compile-time expansion keeps the shared 5x7 glyphs sharp at integer scales.
// No runtime font rasterization or allocation is needed.
// The 7-row cell has no descender space, so lowercase draws with the capital glyphs
// and every label, value and file name shares one baseline.
constexpr std::size_t GlyphSourceIndex(std::size_t index)
{
  return index >= 'a' - 32 && index <= 'z' - 32 ? index - 32 : index;
}

template <int PixelScale, int LineHeight>
struct PixelFont {
  std::array<std::uint8_t, 95 * 35 * PixelScale * PixelScale> bitmap{};
  std::array<RasterGlyph, 95> glyphs{};

  constexpr PixelFont()
  {
    for (std::size_t i = 0; i < glyphs.size(); ++i)
    {
      const std::size_t offset = i * 35 * PixelScale * PixelScale;
      const auto& rows = kTrackerGlyphRows[GlyphSourceIndex(i)];
      glyphs[i] = {static_cast<std::uint32_t>(offset), 5 * PixelScale, 7 * PixelScale,
                   0, (LineHeight - 7 * PixelScale) / 2, 6 * PixelScale};
      for (int y = 0; y < 7 * PixelScale; ++y)
        for (int x = 0; x < 5 * PixelScale; ++x)
          bitmap[offset + y * 5 * PixelScale + x] = (rows[y / PixelScale] & (1 << (4 - x / PixelScale))) ? 255 : 0;
    }
  }
};

constexpr PixelFont<1, 10> kSmall;
constexpr PixelFont<2, 16> kBody;
constexpr PixelFont<3, 28> kValue;
const RasterFont kSmallFont{32, 126, 10, kSmall.bitmap.data(), kSmall.glyphs.data()};
const RasterFont kBodyFont{32, 126, 16, kBody.bitmap.data(), kBody.glyphs.data()};
const RasterFont kValueFont{32, 126, 28, kValue.bitmap.data(), kValue.glyphs.data()};

} // namespace

const RasterFont& UiFontForScale(int scale)
{
  if (scale <= 1) return kSmallFont;
  if (scale <= 3) return kBodyFont;
  return kValueFont;
}

} // namespace ui
