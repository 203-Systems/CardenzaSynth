#pragma once

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>

namespace CardenzaSim::DesktopHost {

inline const COLORREF kBackgroundColor = RGB(8, 10, 12);
inline const COLORREF kScreenColor = RGB(4, 6, 8);
inline const COLORREF kPanelEdgeColor = RGB(60, 65, 70);
inline const COLORREF kScreenGlowColor = RGB(0, 220, 240);
inline const COLORREF kScreenDimColor = RGB(145, 150, 155);
inline const COLORREF kTextColor = RGB(235, 235, 235);
inline const COLORREF kKeyColor = RGB(20, 24, 28);
inline const COLORREF kKeyActiveColor = RGB(236, 244, 250);
inline const COLORREF kLedOffColor = RGB(30, 32, 36);
inline const COLORREF kLedOnColor = RGB(0, 220, 240);
inline const COLORREF kModifierColor = RGB(18, 28, 34);
inline const COLORREF kModifierActiveColor = RGB(0, 88, 102);

COLORREF BlendLedColor(std::uint8_t brightness, std::uint8_t globalBrightness);
void FillRectColor(HDC dc, const RECT& rect, COLORREF color);
void FrameRectColor(HDC dc, const RECT& rect, COLORREF color);
void DrawLine(HDC dc, int x0, int y0, int x1, int y1, COLORREF color);
void DrawRoundPanel(HDC dc, const RECT& rect, COLORREF fill, COLORREF edge, int radius = 12);
void DrawTextBlock(HDC dc, const RECT& rect, const std::string& text, UINT format, COLORREF color, int height, int weight = FW_NORMAL);

}  // namespace CardenzaSim::DesktopHost
