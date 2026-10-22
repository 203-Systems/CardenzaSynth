#pragma once

#define NOMINMAX
#include <windows.h>

#include <cstdint>
#include <string>

namespace CardenzaSim::DesktopHost {

inline const COLORREF kBackgroundColor = RGB(238, 234, 230);
inline const COLORREF kScreenColor = RGB(238, 234, 230);
inline const COLORREF kPanelEdgeColor = RGB(131, 145, 148);
inline const COLORREF kScreenGlowColor = RGB(0, 104, 124);
inline const COLORREF kScreenDimColor = RGB(65, 80, 82);
inline const COLORREF kTextColor = RGB(24, 32, 32);
inline const COLORREF kKeyColor = RGB(222, 222, 222);
inline const COLORREF kKeyActiveColor = RGB(189, 218, 213);
inline const COLORREF kLedOffColor = RGB(131, 145, 148);
inline const COLORREF kLedOnColor = RGB(0, 104, 124);
inline const COLORREF kModifierColor = RGB(205, 210, 205);
inline const COLORREF kModifierActiveColor = RGB(189, 218, 213);

COLORREF BlendLedColor(std::uint8_t brightness, std::uint8_t globalBrightness);
void FillRectColor(HDC dc, const RECT& rect, COLORREF color);
void FrameRectColor(HDC dc, const RECT& rect, COLORREF color);
void DrawLine(HDC dc, int x0, int y0, int x1, int y1, COLORREF color);
void DrawRoundPanel(HDC dc, const RECT& rect, COLORREF fill, COLORREF edge, int radius = 12);
void DrawTextBlock(HDC dc, const RECT& rect, const std::string& text, UINT format, COLORREF color, int height, int weight = FW_NORMAL);

}  // namespace CardenzaSim::DesktopHost
