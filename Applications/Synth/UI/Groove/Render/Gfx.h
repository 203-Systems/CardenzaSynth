#pragma once

#include <cstdint>
#include <string_view>

#include "Devices/Device.h"
#include "OS/UI/Canvas.h"

namespace groove {

using ui::Color;
using ui::Rect;

enum class Font : std::uint8_t {
  Micro,  // 5x7, 6 px advance
  Small,  // 6x9, 7 px advance
  Large,  // 10x14, 12 px advance
  Huge,   // 15x21, 18 px advance
};

enum class Glyph : std::uint8_t {
  Play,
  Stop,
  Rec,
  Minus,
  Plus,
  Enter,
  Up,
  Down,
  Left,
  Right,
  Check,
  Dot,
};

enum class Align : std::uint8_t { Left, Center, Right };

// Direct framebuffer painter: every primitive clips to the screen and the active clip rect.
class Gfx {
public:
  explicit Gfx(Device::Display::MutableFrameBufferView frameBuffer);

  int Width() const { return width_; }
  int Height() const { return height_; }

  void Clear(Color color);
  void SetClip(const Rect& rect);
  void ResetClip();
  const Rect& Clip() const { return clip_; }
  // Every primitive is offset by the origin; clip and dither cells stay in screen space.
  void SetOrigin(int x, int y) { originX_ = x; originY_ = y; }
  int OriginX() const { return originX_; }
  int OriginY() const { return originY_; }
  // Glitch helper: shifts pixels of screen row `y` inside [x, x + width) by `dx` (screen space).
  void ShiftRow(int y, int x, int width, int dx);

  void Pixel(int x, int y, Color color);
  void HLine(int x, int y, int width, Color color);
  void VLine(int x, int y, int height, Color color);
  void Line(int x0, int y0, int x1, int y1, Color color);
  void Fill(const Rect& rect, Color color);
  void Stroke(const Rect& rect, Color color);
  // Ordered 4x4 Bayer dither: `level` of 16 pixels in each cell take `color`, the rest are untouched.
  void Dither(const Rect& rect, Color color, int level);
  // Dotted horizontal rule: one pixel on, `gap` off.
  void DottedHLine(int x, int y, int width, Color color, int gap = 1);
  void DottedVLine(int x, int y, int height, Color color, int gap = 1);

  int Text(int x, int y, std::string_view text, Color color, Font font = Font::Micro);
  int TextAligned(const Rect& rect, std::string_view text, Color color, Font font, Align align);
  // Draws text and clips it to `maxWidth`, ending with '.' when truncated. Returns the end x.
  int TextFit(int x, int y, int maxWidth, std::string_view text, Color color, Font font = Font::Micro);
  static int Measure(std::string_view text, Font font = Font::Micro);
  static int Advance(Font font);
  static int LineHeight(Font font);
  static int CapHeight(Font font);

  void DrawGlyph(int x, int y, Glyph glyph, Color color);
  static int GlyphWidth(Glyph glyph);

private:
  bool Visible(int x, int y) const;

  Device::Display::Pixel* pixels_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  Rect clip_{};
  int originX_ = 0;
  int originY_ = 0;
};

}  // namespace groove
