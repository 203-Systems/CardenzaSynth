#pragma once

#define NOMINMAX
#include <windows.h>

#include <array>
#include <cstddef>
#include <string>

namespace CardenzaSim::DesktopHost {

inline constexpr int kScreenWidth = 240;
inline constexpr int kScreenHeight = 135;
inline constexpr int kPadding = 20;
inline constexpr int kTopBarHeight = 52;
inline constexpr int kScaleButtonWidth = 60;
inline constexpr int kScaleButtonHeight = 28;
inline constexpr int kKeyboardColumns = 14;
inline constexpr int kKeyboardRows = 4;
inline constexpr int kKeyGap = 6;
inline constexpr int kKeyWidth = 54;
inline constexpr int kKeyHeight = 54;
inline constexpr int kKeyboardHeaderHeight = 52;
inline constexpr int kFooterHeight = 24;
inline constexpr int kDefaultScale = 2;

struct KeyVisual {
  int midi = 48;
  int row = 0;
  int column = 0;
  std::string primaryLabel;
  std::string secondaryLabel;
  std::string note;
};

struct MatrixPosition {
  std::size_t row = 0;
  std::size_t column = 0;
};

std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> BuildKeyboard();
MatrixPosition PositionForIndex(std::size_t index);
RECT MakeRect(int left, int top, int width, int height);
bool PointInRect(const RECT& rect, POINT point);

}  // namespace CardenzaSim::DesktopHost
