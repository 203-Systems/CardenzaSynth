#include "OS/UI/Canvas.h"

#include "OS/UI/FontAtlas.h"

#include <algorithm>
#include <string>

namespace ui {

namespace {
const RasterGlyph& GlyphFor(const RasterFont& font, char value)
{
  std::uint8_t codePoint = static_cast<std::uint8_t>(value);
  if (codePoint < font.firstChar || codePoint > font.lastChar)
  {
    codePoint = static_cast<std::uint8_t>('?');
  }
  if (codePoint < font.firstChar || codePoint > font.lastChar)
  {
    codePoint = font.firstChar;
  }
  return font.glyphs[codePoint - font.firstChar];
}

Color BlendRgb565(Color background, Color foreground, std::uint8_t alpha)
{
  if (alpha == 0)
  {
    return background;
  }
  if (alpha == 255)
  {
    return foreground;
  }

  const int bgRed = ((background >> 11) & 0x1F) * 255 / 31;
  const int bgGreen = ((background >> 5) & 0x3F) * 255 / 63;
  const int bgBlue = (background & 0x1F) * 255 / 31;

  const int fgRed = ((foreground >> 11) & 0x1F) * 255 / 31;
  const int fgGreen = ((foreground >> 5) & 0x3F) * 255 / 63;
  const int fgBlue = (foreground & 0x1F) * 255 / 31;

  const int mixedRed = (bgRed * (255 - alpha) + fgRed * alpha) / 255;
  const int mixedGreen = (bgGreen * (255 - alpha) + fgGreen * alpha) / 255;
  const int mixedBlue = (bgBlue * (255 - alpha) + fgBlue * alpha) / 255;

  return Rgb565(static_cast<std::uint8_t>(mixedRed),
                static_cast<std::uint8_t>(mixedGreen),
                static_cast<std::uint8_t>(mixedBlue));
}

}  // namespace

Canvas::Canvas(Device::Display::MutableFrameBufferView frameBuffer)
    : frameBuffer_(frameBuffer) {}

void Canvas::Clear(Color color)
{
  if (frameBuffer_.pixels == nullptr)
  {
    return;
  }

  std::fill(frameBuffer_.pixels, frameBuffer_.pixels + frameBuffer_.pixelCount, color);
}

void Canvas::FillRect(const Rect& rect, Color color)
{
  const int left = std::max(0, rect.x);
  const int top = std::max(0, rect.y);
  const int right = std::min(Width(), rect.x + rect.width);
  const int bottom = std::min(Height(), rect.y + rect.height);
  for (int y = top; y < bottom; ++y)
  {
    for (int x = left; x < right; ++x)
    {
      SetPixel(x, y, color);
    }
  }
}

void Canvas::StrokeRect(const Rect& rect, Color color)
{
  DrawLine(rect.x, rect.y, rect.x + rect.width - 1, rect.y, color);
  DrawLine(rect.x, rect.y + rect.height - 1, rect.x + rect.width - 1, rect.y + rect.height - 1, color);
  DrawLine(rect.x, rect.y, rect.x, rect.y + rect.height - 1, color);
  DrawLine(rect.x + rect.width - 1, rect.y, rect.x + rect.width - 1, rect.y + rect.height - 1, color);
}

void Canvas::DrawLine(int x0, int y0, int x1, int y1, Color color)
{
  const int dx = std::abs(x1 - x0);
  const int sx = x0 < x1 ? 1 : -1;
  const int dy = -std::abs(y1 - y0);
  const int sy = y0 < y1 ? 1 : -1;
  int error = dx + dy;

  while (true)
  {
    SetPixel(x0, y0, color);
    if (x0 == x1 && y0 == y1)
    {
      break;
    }
    const int twiceError = error * 2;
    if (twiceError >= dy)
    {
      error += dy;
      x0 += sx;
    }
    if (twiceError <= dx)
    {
      error += dx;
      y0 += sy;
    }
  }
}

void Canvas::DrawText(int x, int y, std::string_view text, Color color, int scale)
{
  DrawTextClipped(Rect{0, 0, Width(), Height()}, x, y, text, color, scale);
}

void Canvas::DrawTextClipped(const Rect& clipRect, int x, int y, std::string_view text, Color color, int scale)
{
  if (scale < 1)
  {
    scale = 1;
  }

  const int clipLeft = std::max(0, clipRect.x);
  const int clipTop = std::max(0, clipRect.y);
  const int clipRight = std::min(Width(), clipRect.x + clipRect.width);
  const int clipBottom = std::min(Height(), clipRect.y + clipRect.height);
  if (clipLeft >= clipRight || clipTop >= clipBottom)
  {
    return;
  }

  const RasterFont& font = UiFontForScale(scale);
  int cursorX = x;
  int cursorY = y;
  for (char value : text)
  {
    if (value == '\n')
    {
      cursorX = x;
      cursorY += static_cast<int>(font.lineHeight) + 1;
      continue;
    }

    const RasterGlyph& glyph = GlyphFor(font, value);
    const std::uint8_t* glyphBitmap = font.bitmap + glyph.bitmapOffset;
    for (int row = 0; row < static_cast<int>(glyph.height); ++row)
    {
      for (int column = 0; column < static_cast<int>(glyph.width); ++column)
      {
        const std::uint8_t alpha = glyphBitmap[static_cast<std::size_t>(row) * glyph.width + static_cast<std::size_t>(column)];
        if (alpha == 0)
        {
          continue;
        }
        const int pixelX = cursorX + glyph.xOffset + column;
        const int pixelY = cursorY + glyph.yOffset + row;
        if (pixelX < clipLeft || pixelX >= clipRight || pixelY < clipTop || pixelY >= clipBottom)
        {
          continue;
        }
        BlendPixel(pixelX, pixelY, color, alpha);
      }
    }
    cursorX += glyph.xAdvance;
  }
}

void Canvas::DrawTextInRect(const Rect& rect, std::string_view text, Color color, int scale, TextAlign align, VerticalAlign verticalAlign)
{
  int x = rect.x;
  int y = rect.y;
  const int width = MeasureTextWidth(text, scale);
  const int height = FontHeight(scale);

  if (align == TextAlign::Center)
  {
    x = rect.x + (rect.width - width) / 2;
  }
  else if (align == TextAlign::Right)
  {
    x = rect.x + rect.width - width;
  }

  if (verticalAlign == VerticalAlign::Middle)
  {
    y = rect.y + (rect.height - height) / 2;
  }
  else if (verticalAlign == VerticalAlign::Bottom)
  {
    y = rect.y + rect.height - height;
  }

  DrawText(x, y, text, color, scale);
}

int Canvas::MeasureTextWidth(std::string_view text, int scale) const
{
  if (scale < 1)
  {
    scale = 1;
  }

  const RasterFont& font = UiFontForScale(scale);
  int maxWidth = 0;
  int currentWidth = 0;
  for (char value : text)
  {
    if (value == '\n')
    {
      maxWidth = std::max(maxWidth, currentWidth);
      currentWidth = 0;
      continue;
    }
    currentWidth += GlyphFor(font, value).xAdvance;
  }
  return std::max(maxWidth, currentWidth);
}

int Canvas::FontHeight(int scale) const
{
  if (scale < 1)
  {
    scale = 1;
  }
  return static_cast<int>(UiFontForScale(scale).lineHeight);
}

Color Canvas::GetPixel(int x, int y) const
{
  if (frameBuffer_.pixels == nullptr || x < 0 || y < 0 || x >= Width() || y >= Height())
  {
    return 0;
  }

  return frameBuffer_.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(Width()) + static_cast<std::size_t>(x)];
}

void Canvas::BlendPixel(int x, int y, Color color, std::uint8_t alpha)
{
  if (frameBuffer_.pixels == nullptr || x < 0 || y < 0 || x >= Width() || y >= Height())
  {
    return;
  }

  const std::size_t index = static_cast<std::size_t>(y) * static_cast<std::size_t>(Width()) + static_cast<std::size_t>(x);
  frameBuffer_.pixels[index] = BlendRgb565(frameBuffer_.pixels[index], color, alpha);
}

void Canvas::SetPixel(int x, int y, Color color)
{
  if (frameBuffer_.pixels == nullptr || x < 0 || y < 0 || x >= Width() || y >= Height())
  {
    return;
  }

  frameBuffer_.pixels[static_cast<std::size_t>(y) * static_cast<std::size_t>(Width()) + static_cast<std::size_t>(x)] = color;
}

}  // namespace ui