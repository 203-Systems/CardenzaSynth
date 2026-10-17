#include "DesktopSimulator.h"

#include <algorithm>

namespace CardenzaSim::DesktopHost {

RECT DesktopSimulator::ComputeClientRect() const
{
  const int screenWidth = kScreenWidth * scale_;
  const int screenHeight = kScreenHeight * scale_;
  const int keyGridWidth = kKeyboardColumns * kKeyWidth + (kKeyboardColumns - 1) * kKeyGap;
  const int keyGridHeight = kKeyboardRows * kKeyHeight + (kKeyboardRows - 1) * kKeyGap;
  const int contentWidth = std::max(screenWidth, keyGridWidth) + kPadding * 2;
  const int contentHeight = kTopBarHeight + screenHeight + kPadding + kKeyboardHeaderHeight + keyGridHeight + kPadding + kFooterHeight + kPadding;
  return MakeRect(0, 0, contentWidth, contentHeight);
}

void DesktopSimulator::UpdateLayout()
{
  RECT clientRect{};
  GetClientRect(hwnd_, &clientRect);
  const int clientWidth = clientRect.right - clientRect.left;

  int currentX = kPadding;
  const int currentY = 14;
  for (std::size_t index = 0; index < scaleButtons_.size(); ++index)
  {
    scaleButtons_[index] = MakeRect(currentX, currentY, kScaleButtonWidth, kScaleButtonHeight);
    currentX += kScaleButtonWidth + 8;
  }
  homeButton_ = MakeRect(currentX, currentY, 88, kScaleButtonHeight);

  const int screenWidth = kScreenWidth * scale_;
  const int screenHeight = kScreenHeight * scale_;
  const int screenLeft = (clientWidth - screenWidth) / 2;
  screenRect_ = MakeRect(screenLeft, kTopBarHeight, screenWidth, screenHeight);

  keyboardHeaderRect_ = MakeRect(kPadding, screenRect_.bottom + kPadding,
                                 clientWidth - kPadding * 2, kKeyboardHeaderHeight);

  const int keyGridWidth = kKeyboardColumns * kKeyWidth + (kKeyboardColumns - 1) * kKeyGap;
  const int keyLeft = (clientWidth - keyGridWidth) / 2;
  const int keyTop = keyboardHeaderRect_.bottom + 8;
  for (int row = 0; row < kKeyboardRows; ++row)
  {
    for (int column = 0; column < kKeyboardColumns; ++column)
    {
      const int index = row * kKeyboardColumns + column;
      keyButtons_[index] = MakeRect(
          keyLeft + column * (kKeyWidth + kKeyGap),
          keyTop + row * (kKeyHeight + kKeyGap),
          kKeyWidth,
          kKeyHeight);
    }
  }

  footerRect_ = MakeRect(kPadding, keyTop + kKeyboardRows * kKeyHeight + (kKeyboardRows - 1) * kKeyGap + kPadding,
                         clientWidth - kPadding * 2, kFooterHeight);
}

void DesktopSimulator::ResizeForScale()
{
  RECT clientRect = ComputeClientRect();
  RECT windowRect = clientRect;
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
  SetWindowPos(hwnd_, nullptr, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
               SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
  UpdateLayout();
  RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME);
}

int DesktopSimulator::ScreenPx(int value) const
{
  return value * scale_;
}

}  // namespace CardenzaSim::DesktopHost
