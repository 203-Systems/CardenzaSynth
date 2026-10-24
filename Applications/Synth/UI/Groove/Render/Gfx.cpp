#include "Applications/Synth/UI/Groove/Render/Gfx.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "OS/UI/FontAtlas.h"

namespace groove {

namespace {

constexpr std::array<std::array<std::uint8_t, 4>, 4> kBayer = {{
    {{0, 8, 2, 10}},
    {{12, 4, 14, 6}},
    {{3, 11, 1, 9}},
    {{15, 7, 13, 5}},
}};

const ui::RasterFont& FontAtlas(Font font)
{
  switch (font)
  {
    case Font::Micro:
      return ui::UiMicroFont();
    case Font::Small:
      return ui::UiFontForScale(1);
    case Font::Large:
      return ui::UiFontForScale(2);
    case Font::Huge:
    default:
      return ui::UiFontForScale(4);
  }
}

const ui::RasterGlyph& GlyphFor(const ui::RasterFont& font, char value)
{
  auto code = static_cast<std::uint8_t>(value);
  if (code < font.firstChar || code > font.lastChar) code = static_cast<std::uint8_t>('?');
  return font.glyphs[code - font.firstChar];
}

struct GlyphBitmap {
  std::uint8_t width;
  std::array<std::uint8_t, 7> rows;  // MSB-first within `width` bits
};

constexpr GlyphBitmap BitmapFor(Glyph glyph)
{
  switch (glyph)
  {
    case Glyph::Play:
      return {5, {{0b10000, 0b11000, 0b11100, 0b11110, 0b11100, 0b11000, 0b10000}}};
    case Glyph::Stop:
      return {5, {{0, 0b11111, 0b11111, 0b11111, 0b11111, 0b11111, 0}}};
    case Glyph::Rec:
      return {5, {{0, 0b01110, 0b11111, 0b11111, 0b11111, 0b01110, 0}}};
    case Glyph::Minus:
      return {5, {{0, 0, 0, 0b11111, 0, 0, 0}}};
    case Glyph::Plus:
      return {5, {{0, 0b00100, 0b00100, 0b11111, 0b00100, 0b00100, 0}}};
    case Glyph::Enter:
      return {5, {{0b00001, 0b00001, 0b00101, 0b01101, 0b11111, 0b01100, 0b00100}}};
    case Glyph::Up:
      return {5, {{0b00100, 0b01110, 0b11111, 0b00100, 0b00100, 0b00100, 0b00100}}};
    case Glyph::Down:
      return {5, {{0b00100, 0b00100, 0b00100, 0b00100, 0b11111, 0b01110, 0b00100}}};
    case Glyph::Left:
      return {7, {{0, 0b0010000, 0b0110000, 0b1111111, 0b0110000, 0b0010000, 0}}};
    case Glyph::Right:
      return {7, {{0, 0b0000100, 0b0000110, 0b1111111, 0b0000110, 0b0000100, 0}}};
    case Glyph::Check:
      return {5, {{0, 0b00001, 0b00010, 0b10100, 0b01000, 0, 0}}};
    case Glyph::Dot:
    default:
      return {3, {{0, 0, 0b010, 0b111, 0b010, 0, 0}}};
  }
}

}  // namespace

Gfx::Gfx(Device::Display::MutableFrameBufferView frameBuffer)
    : pixels_(frameBuffer.pixels), width_(frameBuffer.width), height_(frameBuffer.height)
{
  ResetClip();
}

void Gfx::Clear(Color color)
{
  if (pixels_ == nullptr) return;
  std::fill(pixels_, pixels_ + static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), color);
}

void Gfx::SetClip(const Rect& rect)
{
  const int left = std::max(0, rect.x);
  const int top = std::max(0, rect.y);
  const int right = std::min(width_, rect.x + rect.width);
  const int bottom = std::min(height_, rect.y + rect.height);
  clip_ = Rect{left, top, std::max(0, right - left), std::max(0, bottom - top)};
}

void Gfx::ResetClip()
{
  clip_ = Rect{0, 0, width_, height_};
}

bool Gfx::Visible(int x, int y) const
{
  return pixels_ != nullptr && x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.width && y < clip_.y + clip_.height;
}

void Gfx::Pixel(int x, int y, Color color)
{
  x += originX_;
  y += originY_;
  if (Visible(x, y)) pixels_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(x)] = color;
}

void Gfx::HLine(int x, int y, int width, Color color)
{
  Fill(Rect{x, y, width, 1}, color);
}

void Gfx::VLine(int x, int y, int height, Color color)
{
  Fill(Rect{x, y, 1, height}, color);
}

void Gfx::Line(int x0, int y0, int x1, int y1, Color color)
{
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;
  for (;;)
  {
    Pixel(x0, y0, color);
    if (x0 == x1 && y0 == y1) break;
    const int twice = error * 2;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
  }
}

void Gfx::Fill(const Rect& area, Color color)
{
  if (pixels_ == nullptr) return;
  const Rect rect{area.x + originX_, area.y + originY_, area.width, area.height};
  const int left = std::max(clip_.x, rect.x);
  const int top = std::max(clip_.y, rect.y);
  const int right = std::min(clip_.x + clip_.width, rect.x + rect.width);
  const int bottom = std::min(clip_.y + clip_.height, rect.y + rect.height);
  for (int y = top; y < bottom; ++y)
  {
    auto* row = pixels_ + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_);
    std::fill(row + left, row + std::max(left, right), color);
  }
}

void Gfx::Stroke(const Rect& rect, Color color)
{
  if (rect.width <= 0 || rect.height <= 0) return;
  HLine(rect.x, rect.y, rect.width, color);
  HLine(rect.x, rect.y + rect.height - 1, rect.width, color);
  VLine(rect.x, rect.y, rect.height, color);
  VLine(rect.x + rect.width - 1, rect.y, rect.height, color);
}

void Gfx::Dither(const Rect& area, Color color, int level)
{
  if (pixels_ == nullptr || level <= 0) return;
  if (level >= 16)
  {
    Fill(area, color);
    return;
  }
  const Rect rect{area.x + originX_, area.y + originY_, area.width, area.height};
  const int left = std::max(clip_.x, rect.x);
  const int top = std::max(clip_.y, rect.y);
  const int right = std::min(clip_.x + clip_.width, rect.x + rect.width);
  const int bottom = std::min(clip_.y + clip_.height, rect.y + rect.height);
  for (int y = top; y < bottom; ++y)
  {
    auto* row = pixels_ + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_);
    const auto& thresholds = kBayer[static_cast<std::size_t>(y & 3)];
    for (int x = left; x < right; ++x)
      if (thresholds[static_cast<std::size_t>(x & 3)] < level) row[x] = color;
  }
}

void Gfx::ShiftRow(int y, int x, int width, int dx)
{
  if (pixels_ == nullptr || y < 0 || y >= height_ || dx == 0) return;
  const int left = std::max(0, x);
  const int right = std::min(width_, x + width);
  if (right - left <= std::abs(dx)) return;
  auto* row = pixels_ + static_cast<std::size_t>(y) * static_cast<std::size_t>(width_);
  if (dx > 0) std::copy_backward(row + left, row + right - dx, row + right);
  else std::copy(row + left - dx, row + right, row + left);
}

void Gfx::DottedHLine(int x, int y, int width, Color color, int gap)
{
  for (int offset = 0; offset < width; offset += 1 + gap) Pixel(x + offset, y, color);
}

void Gfx::DottedVLine(int x, int y, int height, Color color, int gap)
{
  for (int offset = 0; offset < height; offset += 1 + gap) Pixel(x, y + offset, color);
}

int Gfx::Text(int x, int y, std::string_view text, Color color, Font font)
{
  const ui::RasterFont& atlas = FontAtlas(font);
  const int baseOffset = GlyphFor(atlas, 'A').yOffset;
  int cursor = x;
  for (const char value : text)
  {
    const ui::RasterGlyph& glyph = GlyphFor(atlas, value);
    const std::uint8_t* bitmap = atlas.bitmap + glyph.bitmapOffset;
    const int top = y + glyph.yOffset - baseOffset;
    for (int row = 0; row < glyph.height; ++row)
      for (int column = 0; column < glyph.width; ++column)
        if (bitmap[static_cast<std::size_t>(row) * glyph.width + static_cast<std::size_t>(column)] > 127)
          Pixel(cursor + glyph.xOffset + column, top + row, color);
    cursor += glyph.xAdvance;
  }
  return cursor;
}

int Gfx::TextAligned(const Rect& rect, std::string_view text, Color color, Font font, Align align)
{
  const int width = Measure(text, font);
  int x = rect.x;
  if (align == Align::Center) x = rect.x + (rect.width - width) / 2;
  else if (align == Align::Right) x = rect.x + rect.width - width;
  const int y = rect.y + (rect.height - CapHeight(font)) / 2;
  return Text(x, y, text, color, font);
}

int Gfx::TextFit(int x, int y, int maxWidth, std::string_view text, Color color, Font font)
{
  const int advance = Advance(font);
  if (maxWidth < advance) return x;
  if (Measure(text, font) <= maxWidth) return Text(x, y, text, color, font);
  const std::size_t keep = static_cast<std::size_t>(std::max(0, maxWidth / advance - 1));
  const int end = Text(x, y, text.substr(0, keep), color, font);
  return Text(end, y, ".", color, font);
}

int Gfx::Measure(std::string_view text, Font font)
{
  return static_cast<int>(text.size()) * Advance(font);
}

int Gfx::Advance(Font font)
{
  return GlyphFor(FontAtlas(font), 'A').xAdvance;
}

int Gfx::LineHeight(Font font)
{
  switch (font)
  {
    case Font::Micro: return 9;
    case Font::Small: return 11;
    case Font::Large: return 16;
    case Font::Huge:
    default: return 24;
  }
}

int Gfx::CapHeight(Font font)
{
  return GlyphFor(FontAtlas(font), 'A').height;
}

void Gfx::DrawGlyph(int x, int y, Glyph glyph, Color color)
{
  const GlyphBitmap bitmap = BitmapFor(glyph);
  for (int row = 0; row < 7; ++row)
    for (int column = 0; column < bitmap.width; ++column)
      if (bitmap.rows[static_cast<std::size_t>(row)] & (1u << (bitmap.width - 1 - column))) Pixel(x + column, y + row, color);
}

int Gfx::GlyphWidth(Glyph glyph)
{
  return BitmapFor(glyph).width;
}

}  // namespace groove
