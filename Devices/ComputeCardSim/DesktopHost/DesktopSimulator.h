#pragma once

#define NOMINMAX
#include <windows.h>

#include <array>
#include <cstdint>
#include <string>

#include "DesktopHostLayout.h"

namespace apps {
class SynthApp;
}

namespace ComputeCardSim::DesktopHost {

inline constexpr UINT_PTR kFrameTimerId = 1;
inline constexpr UINT kFrameTimerPeriodMs = 16;

class DesktopSimulator {
public:
  explicit DesktopSimulator(int initialScale);

  bool Initialize();
  bool Create(HINSTANCE instance);
  int Run();

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);
  RECT ComputeClientRect() const;
  void UpdateLayout();
  void ResizeForScale();
  int ScreenPx(int value) const;

  void OnFrameTick();
  void OnLeftButtonDown(int x, int y);
  void OnMouseMove(int x, int y, UINT flags);
  void OnLeftButtonUp();
  void OnKeyDown(UINT virtualKey);

  bool ShouldCaptureKey(MatrixPosition position) const;
  void SetVirtualKey(MatrixPosition position, bool pressed);
  void TapVirtualKey(MatrixPosition position);

  std::string FooterText() const;
  std::string KeyboardHeaderText() const;
  std::size_t ContextMenuFirstColumn() const;
  std::size_t ContextMenuColumnIndex(std::size_t column) const;
  bool IsMenuKey(std::size_t row, std::size_t column) const;
  bool IsHomeRightBarKey(std::size_t column) const;

  COLORREF KeyFillColor(std::size_t index, bool active) const;
  COLORREF KeyEdgeColor(std::size_t index, bool active) const;
  COLORREF KeyLedColor(std::size_t index,
                       bool deviceLedsEnabled,
                       std::uint8_t deviceLedGlobalBrightness) const;
  void DrawFrameBuffer(HDC dc);
  void DrawScaleButtons(HDC dc);
  void DrawKeyboard(HDC dc);
  void DrawFooter(HDC dc);
  void OnPaint();

  HWND hwnd_ = nullptr;
  int scale_ = kDefaultScale;
  int capturedKey_ = -1;
  int dragMenuIndex_ = -1;
  ULONGLONG lastFrameTick_ = 0;
  apps::SynthApp* synthApp_ = nullptr;
  std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> keyboard_{};
  std::array<RECT, 3> scaleButtons_{};
  std::array<RECT, kKeyboardRows * kKeyboardColumns> keyButtons_{};
  RECT screenRect_{};
  RECT keyboardHeaderRect_{};
  RECT footerRect_{};
};

}  // namespace ComputeCardSim::DesktopHost
