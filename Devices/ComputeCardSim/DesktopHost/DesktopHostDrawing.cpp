#include "DesktopHostDrawing.h"

#include <algorithm>

#include "Devices/Device.h"

namespace ComputeCardSim::DesktopHost {

COLORREF BlendLedColor(std::uint8_t brightness, std::uint8_t globalBrightness)
{
  const std::uint8_t effectiveBrightness = std::min(brightness, globalBrightness);
  if (effectiveBrightness == Device::LED::kBrightnessOff)
  {
    return kLedOffColor;
  }

  const auto blendChannel = [effectiveBrightness](BYTE offChannel, BYTE onChannel) -> BYTE {
    const unsigned int offWeight = static_cast<unsigned int>(Device::LED::kBrightnessMax - effectiveBrightness);
    const unsigned int onWeight = static_cast<unsigned int>(effectiveBrightness);
    return static_cast<BYTE>((static_cast<unsigned int>(offChannel) * offWeight + static_cast<unsigned int>(onChannel) * onWeight) /
                             static_cast<unsigned int>(Device::LED::kBrightnessMax));
  };

  return RGB(blendChannel(GetRValue(kLedOffColor), GetRValue(kLedOnColor)),
             blendChannel(GetGValue(kLedOffColor), GetGValue(kLedOnColor)),
             blendChannel(GetBValue(kLedOffColor), GetBValue(kLedOnColor)));
}

void FillRectColor(HDC dc, const RECT& rect, COLORREF color)
{
  const HBRUSH brush = CreateSolidBrush(color);
  FillRect(dc, &rect, brush);
  DeleteObject(brush);
}

void FrameRectColor(HDC dc, const RECT& rect, COLORREF color)
{
  const HBRUSH brush = CreateSolidBrush(color);
  FrameRect(dc, &rect, brush);
  DeleteObject(brush);
}

void DrawLine(HDC dc, int x0, int y0, int x1, int y1, COLORREF color)
{
  const HPEN pen = CreatePen(PS_SOLID, 1, color);
  const HGDIOBJ oldPen = SelectObject(dc, pen);
  MoveToEx(dc, x0, y0, nullptr);
  LineTo(dc, x1, y1);
  SelectObject(dc, oldPen);
  DeleteObject(pen);
}

void DrawRoundPanel(HDC dc, const RECT& rect, COLORREF fill, COLORREF edge, int radius)
{
  const HBRUSH fillBrush = CreateSolidBrush(fill);
  const HPEN pen = CreatePen(PS_SOLID, 1, edge);
  const HGDIOBJ oldBrush = SelectObject(dc, fillBrush);
  const HGDIOBJ oldPen = SelectObject(dc, pen);
  RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
  SelectObject(dc, oldBrush);
  SelectObject(dc, oldPen);
  DeleteObject(fillBrush);
  DeleteObject(pen);
}

void DrawTextBlock(HDC dc, const RECT& rect, const std::string& text, UINT format, COLORREF color, int height, int weight)
{
  HFONT font = CreateFontA(height, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, "Segoe UI");
  const HGDIOBJ oldFont = SelectObject(dc, font);
  const COLORREF oldColor = SetTextColor(dc, color);
  const int oldMode = SetBkMode(dc, TRANSPARENT);
  DrawTextA(dc, text.c_str(), static_cast<int>(text.size()), const_cast<RECT*>(&rect), format);
  SetBkMode(dc, oldMode);
  SetTextColor(dc, oldColor);
  SelectObject(dc, oldFont);
  DeleteObject(font);
}

}  // namespace ComputeCardSim::DesktopHost
