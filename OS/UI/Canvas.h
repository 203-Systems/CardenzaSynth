#pragma once

#include <cstdint>
#include <string_view>

#include "Devices/Device.h"

namespace ui {

using Color = Device::Display::Pixel;

struct Rect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

enum class TextAlign {
  Left,
  Center,
  Right,
};

enum class VerticalAlign {
  Top,
  Middle,
  Bottom,
};

constexpr Color Rgb565(std::uint8_t red, std::uint8_t green, std::uint8_t blue) {
  return static_cast<Color>(((red & 0xF8u) << 8) | ((green & 0xFCu) << 3) | (blue >> 3));
}

class Canvas {
public:
  explicit Canvas(Device::Display::MutableFrameBufferView frameBuffer);

  int Width() const { return frameBuffer_.width; }
  int Height() const { return frameBuffer_.height; }

  void Clear(Color color);
  void FillRect(const Rect& rect, Color color);
  void StrokeRect(const Rect& rect, Color color);
  void DrawLine(int x0, int y0, int x1, int y1, Color color);
  void DrawText(int x, int y, std::string_view text, Color color, int scale = 1);
  void DrawTextClipped(const Rect& clipRect, int x, int y, std::string_view text, Color color, int scale = 1);
  void DrawTextInRect(const Rect& rect, std::string_view text, Color color, int scale = 1,
                      TextAlign align = TextAlign::Left, VerticalAlign verticalAlign = VerticalAlign::Top);
  int MeasureTextWidth(std::string_view text, int scale = 1) const;
  int FontHeight(int scale = 1) const;

private:
  Color GetPixel(int x, int y) const;
  void BlendPixel(int x, int y, Color color, std::uint8_t alpha);
  void SetPixel(int x, int y, Color color);

  Device::Display::MutableFrameBufferView frameBuffer_;
};

}  // namespace ui