#define NOMINMAX
#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Applications/BuiltInApplications.h"
#include "Applications/Synth/SynthApp.h"
#include "Devices/Device.h"
#include "Library/Synth/Core/ISynthEngine.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/Framework/Midi/MidiPacket.h"
#include "OS/MIDI/MIDI.h"

namespace {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr int kPadding = 20;
constexpr int kTopBarHeight = 52;
constexpr int kScaleButtonWidth = 60;
constexpr int kScaleButtonHeight = 28;
constexpr int kKeyboardColumns = 14;
constexpr int kKeyboardRows = 4;
constexpr int kKeyGap = 6;
constexpr int kKeyWidth = 54;
constexpr int kKeyHeight = 54;
constexpr int kKeyboardHeaderHeight = 52;
constexpr int kFooterHeight = 24;
constexpr int kDefaultScale = 2;
constexpr UINT_PTR kFrameTimerId = 1;
constexpr UINT kFrameTimerPeriodMs = 16;

constexpr COLORREF kBackgroundColor = RGB(8, 10, 12);
constexpr COLORREF kScreenColor = RGB(4, 6, 8);
constexpr COLORREF kPanelEdgeColor = RGB(60, 65, 70);
constexpr COLORREF kScreenGlowColor = RGB(0, 220, 240);
constexpr COLORREF kScreenDimColor = RGB(145, 150, 155);
constexpr COLORREF kTextColor = RGB(235, 235, 235);
constexpr COLORREF kKeyColor = RGB(20, 24, 28);
constexpr COLORREF kKeyActiveColor = RGB(236, 244, 250);
constexpr COLORREF kLedOffColor = RGB(30, 32, 36);
constexpr COLORREF kLedOnColor = RGB(0, 220, 240);
constexpr COLORREF kAccentDimColor = RGB(0, 110, 126);
constexpr COLORREF kModifierColor = RGB(18, 28, 34);
constexpr COLORREF kModifierActiveColor = RGB(0, 88, 102);
constexpr COLORREF kDarkLineColor = RGB(35, 38, 42);

struct KeyVisual {
  int midi = 48;
  int row = 0;
  int column = 0;
  std::string primaryLabel;
  std::string secondaryLabel;
  std::string note;
};

struct KeyLegend {
  const char* primary;
  const char* secondary;
};

struct MatrixPosition {
  std::size_t row = 0;
  std::size_t column = 0;
};

constexpr std::array<std::array<KeyLegend, kKeyboardColumns>, kKeyboardRows> kKeyLegends = {{
    {{{"TRK", ""}, {"1", "!"}, {"2", "@"}, {"3", "#"}, {"4", "$"}, {"5", "%"}, {"6", "^"},
      {"7", "&"}, {"8", "*"}, {"9", "("}, {"0", ")"}, {"-", "_"}, {"=", "+"}, {"del", ""}}},
    {{{"SND", ""}, {"Q", ""}, {"W", ""}, {"E", ""}, {"R", ""}, {"T", ""}, {"Y", ""},
      {"U", ""}, {"I", ""}, {"O", ""}, {"P", ""}, {"[", "{"}, {"]", "}"}, {"\\", "|"}}},
    {{{"LAY", ""}, {"Aa", ""}, {"A", ""}, {"S", ""}, {"D", ""}, {"F", ""}, {"G", ""},
      {"H", ""}, {"J", ""}, {"K", ""}, {"L", ""}, {";", ":"}, {"'", "\""}, {"ok", "enter"}}},
    {{{"SYS", ""}, {"opt", ""}, {"alt", ""}, {"Z", ""}, {"X", ""}, {"C", ""}, {"V", ""},
      {"B", ""}, {"N", ""}, {"M", ""}, {",", "<"}, {".", ">"}, {"/", "?"}, {"space", ""}}},
}};

  constexpr std::array<const char*, 4> kLeftBarLabels = {"TRK", "SND", "LAY", "SYS"};
constexpr std::array<const char*, 4> kRightBarLabels = {"OCT+", "OCT-", "REC", "PLAY"};

std::string NoteName(int midi)
{
  static constexpr const char* kNoteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int octave = midi / 12 - 1;
  std::ostringstream stream;
  stream << kNoteNames[midi % 12] << octave;
  return stream.str();
}

KeyVisual BuildKeyVisual(std::size_t index)
{
  KeyVisual key;
  key.row = static_cast<int>(index / kKeyboardColumns);
  key.column = static_cast<int>(index % kKeyboardColumns);
  const bool isPerformanceKey = key.column >= static_cast<int>(synth::kHomePerformanceFirstColumn) &&
                                key.column <= static_cast<int>(synth::kHomePerformanceLastColumn);
  key.midi = isPerformanceKey ? static_cast<int>(synth::HomePerformanceMidiNote(static_cast<std::size_t>(key.row),
                                                                                 static_cast<std::size_t>(key.column)))
                              : 48 + static_cast<int>(index);
  const auto& legend = kKeyLegends[key.row][key.column];
  key.note = isPerformanceKey ? NoteName(key.midi) : std::string{};
  key.primaryLabel = legend.primary;
  key.secondaryLabel = legend.secondary;
  return key;
}

std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> BuildKeyboard()
{
  std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> keys{};
  for (std::size_t index = 0; index < keys.size(); ++index)
  {
    keys[index] = BuildKeyVisual(index);
  }
  return keys;
}

MatrixPosition PositionForIndex(std::size_t index)
{
  return MatrixPosition{index / kKeyboardColumns, index % kKeyboardColumns};
}

int ParseInitialScale(const std::string& commandLine)
{
  const std::string token = "--scale=";
  const auto position = commandLine.find(token);
  if (position == std::string::npos)
  {
    return kDefaultScale;
  }

  const auto start = position + token.size();
  const auto end = commandLine.find(' ', start);
  const std::string value = commandLine.substr(start, end == std::string::npos ? std::string::npos : end - start);
  if (value == "1" || value == "2" || value == "4")
  {
    return std::stoi(value);
  }
  return kDefaultScale;
}

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

RECT MakeRect(int left, int top, int width, int height)
{
  RECT rect{};
  rect.left = left;
  rect.top = top;
  rect.right = left + width;
  rect.bottom = top + height;
  return rect;
}

RECT OffsetRectCopy(RECT rect, int dx, int dy)
{
  OffsetRect(&rect, dx, dy);
  return rect;
}

bool PointInRect(const RECT& rect, POINT point)
{
  return point.x >= rect.left && point.x < rect.right && point.y >= rect.top && point.y < rect.bottom;
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

void DrawRoundPanel(HDC dc, const RECT& rect, COLORREF fill, COLORREF edge, int radius = 12)
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

void DrawTextBlock(HDC dc, const RECT& rect, const std::string& text, UINT format, COLORREF color, int height, int weight = FW_NORMAL)
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

class DesktopSimulator {
public:
  explicit DesktopSimulator(int initialScale)
      : scale_(initialScale), keyboard_(BuildKeyboard()) {}

  bool Initialize()
  {
    app::RegisterBuiltInApplications();
    application_ = app::CreateDefaultApplication();
    if (!application_)
    {
      MessageBoxA(nullptr, "Failed to create Synth application.", "ComputeCardSimDesktop", MB_ICONERROR | MB_OK);
      return false;
    }

    synthApp_ = dynamic_cast<apps::SynthApp*>(application_.get());
    if (synthApp_ == nullptr)
    {
      MessageBoxA(nullptr, "Synth application registry returned an unexpected type.", "ComputeCardSimDesktop", MB_ICONERROR | MB_OK);
      return false;
    }

    os::midi::Initialize();
    synthApp_->Setup({});
    UpdateApplicationLeds();
    application_->RenderFrame();
    Device::Display::Present();
    return true;
  }

  bool Create(HINSTANCE instance)
  {
    WNDCLASSA windowClass{};
    windowClass.lpfnWndProc = &DesktopSimulator::WndProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = "ComputeCardSimDesktopWindow";
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassA(&windowClass);

    RECT clientRect = ComputeClientRect();
    RECT windowRect = clientRect;
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);

    hwnd_ = CreateWindowExA(
        0,
        windowClass.lpszClassName,
        "ComputeCardSim Desktop",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr,
        nullptr,
        instance,
        this);

    if (hwnd_ != nullptr)
    {
      lastFrameTick_ = GetTickCount64();
      SetTimer(hwnd_, kFrameTimerId, kFrameTimerPeriodMs, nullptr);
    }
    return hwnd_ != nullptr;
  }

  int Run()
  {
    ShowWindow(hwnd_, SW_SHOWDEFAULT);
    UpdateWindow(hwnd_);

    MSG message{};
    while (GetMessageA(&message, nullptr, 0, 0) > 0)
    {
      TranslateMessage(&message);
      DispatchMessageA(&message);
    }
    return static_cast<int>(message.wParam);
  }

private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
  {
    auto* simulator = reinterpret_cast<DesktopSimulator*>(GetWindowLongPtrA(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
      const auto* create = reinterpret_cast<CREATESTRUCTA*>(lParam);
      simulator = reinterpret_cast<DesktopSimulator*>(create->lpCreateParams);
      simulator->hwnd_ = hwnd;
      SetWindowLongPtrA(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(simulator));
    }

    if (!simulator)
    {
      return DefWindowProcA(hwnd, message, wParam, lParam);
    }

    return simulator->HandleMessage(message, wParam, lParam);
  }

  LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
  {
    switch (message)
    {
      case WM_PAINT:
        OnPaint();
        return 0;
      case WM_LBUTTONDOWN:
        OnLeftButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
      case WM_MOUSEMOVE:
        OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam), static_cast<UINT>(wParam));
        return 0;
      case WM_LBUTTONUP:
        OnLeftButtonUp();
        return 0;
      case WM_KEYDOWN:
        OnKeyDown(static_cast<UINT>(wParam));
        return 0;
      case WM_TIMER:
        if (wParam == kFrameTimerId)
        {
          OnFrameTick();
          return 0;
        }
        return 0;
      case WM_DESTROY:
        KillTimer(hwnd_, kFrameTimerId);
        PostQuitMessage(0);
        return 0;
      default:
        return DefWindowProcA(hwnd_, message, wParam, lParam);
    }
  }

  RECT ComputeClientRect() const
  {
    const int screenWidth = kScreenWidth * scale_;
    const int screenHeight = kScreenHeight * scale_;
    const int keyGridWidth = kKeyboardColumns * kKeyWidth + (kKeyboardColumns - 1) * kKeyGap;
    const int keyGridHeight = kKeyboardRows * kKeyHeight + (kKeyboardRows - 1) * kKeyGap;
    const int contentWidth = std::max(screenWidth, keyGridWidth) + kPadding * 2;
    const int contentHeight = kTopBarHeight + screenHeight + kPadding + kKeyboardHeaderHeight + keyGridHeight + kPadding + kFooterHeight + kPadding;
    return MakeRect(0, 0, contentWidth, contentHeight);
  }

  void UpdateLayout()
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

  void ResizeForScale()
  {
    RECT clientRect = ComputeClientRect();
    RECT windowRect = clientRect;
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX, FALSE);
    SetWindowPos(hwnd_, nullptr, 0, 0, windowRect.right - windowRect.left, windowRect.bottom - windowRect.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateLayout();
    RedrawWindow(hwnd_, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ERASE | RDW_FRAME);
  }

  int ScreenPx(int value) const
  {
    return value * scale_;
  }

  void UpdateApplicationLeds()
  {
    Device::LED::SetEnabled(false);
    Device::LED::Clear();
    application_->UpdateLeds();
  }

  void OnFrameTick()
  {
    lastFrameTick_ = GetTickCount64();

    application_->Tick();
    UpdateApplicationLeds();
    application_->RenderFrame();
    Device::Display::Present();

    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void OnLeftButtonDown(int x, int y)
  {
    const POINT point{x, y};
    UpdateLayout();

    for (std::size_t index = 0; index < scaleButtons_.size(); ++index)
    {
      if (PointInRect(scaleButtons_[index], point))
      {
        const int nextScale = index == 0 ? 1 : (index == 1 ? 2 : 4);
        if (nextScale != scale_)
        {
          scale_ = nextScale;
          ResizeForScale();
        }
        return;
      }
    }

    for (std::size_t index = 0; index < keyButtons_.size(); ++index)
    {
      if (PointInRect(keyButtons_[index], point))
      {
        const auto position = PositionForIndex(index);
        const auto handling = HandleVirtualKeyDown(index);
        if (handling == synth::MatrixKeyHandling::ModifierHold ||
            handling == synth::MatrixKeyHandling::PerformanceKey ||
            handling == synth::MatrixKeyHandling::MenuAction ||
            position.column == synth::kHomeRightBarColumn)
        {
          SetCapture(hwnd_);
          capturedKey_ = static_cast<int>(index);
        }
        InvalidateRect(hwnd_, nullptr, FALSE);
        return;
      }
    }
  }

  void OnMouseMove(int x, int y, UINT flags)
  {
    if ((flags & MK_LBUTTON) == 0 || capturedKey_ < 0)
    {
      return;
    }

    const auto capturedPosition = PositionForIndex(static_cast<std::size_t>(capturedKey_));
    if (capturedPosition.column != synth::kHomeLeftBarColumn)
    {
      return;
    }

    const POINT point{x, y};
    for (std::size_t index = 0; index < keyButtons_.size(); ++index)
    {
      if (!PointInRect(keyButtons_[index], point))
      {
        continue;
      }

      if (dragMenuIndex_ == static_cast<int>(index))
      {
        return;
      }

      const auto position = PositionForIndex(index);
      if (!IsMenuKey(position.row, position.column))
      {
        dragMenuIndex_ = -1;
        return;
      }

      if (synthApp_->PressMatrixKey(position.row, position.column) == synth::MatrixKeyHandling::MenuAction)
      {
        dragMenuIndex_ = static_cast<int>(index);
        InvalidateRect(hwnd_, nullptr, FALSE);
      }
      return;
    }

    dragMenuIndex_ = -1;
  }

  void OnLeftButtonUp()
  {
    if (capturedKey_ < 0)
    {
      return;
    }

    const auto position = PositionForIndex(static_cast<std::size_t>(capturedKey_));
    if (position.column == 0)
    {
      synthApp_->ReleaseMatrixKey(position.row, position.column);
    }
    else
    {
      synthApp_->ReleaseMatrixKey(position.row, position.column);
    }

    capturedKey_ = -1;
    dragMenuIndex_ = -1;
    ReleaseCapture();
    InvalidateRect(hwnd_, nullptr, FALSE);
  }

  void OnKeyDown(UINT virtualKey)
  {
    if (virtualKey == '1' || virtualKey == '2' || virtualKey == '4')
    {
      const int nextScale = virtualKey == '1' ? 1 : (virtualKey == '2' ? 2 : 4);
      if (nextScale != scale_)
      {
        scale_ = nextScale;
        ResizeForScale();
      }
    }
  }

  synth::MatrixKeyHandling HandleVirtualKeyDown(std::size_t keyIndex)
  {
    const auto position = PositionForIndex(keyIndex);
    return synthApp_->PressMatrixKey(position.row, position.column);
  }

  std::string FooterText() const
  {
    return "Scale: 1 / 2 / 4. Home: left edge = track/modifier bar, middle 12 keys = one octave per row, right edge = action bar. Context: hold left edge and use the two right-most columns as the menu.";
  }

  std::string KeyboardHeaderText() const
  {
    if (synthApp_->Ui().Mode() == synth::UiMode::ContextMenu)
    {
      return "Context mode: release the held left key to cancel, or choose a right-side item to keep this page latched.";
    }
    return "Home mode: left column switches tracks or opens modifiers, the center 12 keys perform, and the right column is reserved for home actions.";
  }

  std::size_t ContextMenuFirstColumn() const
  {
    return synthApp_->Ui().ContextMenuColumnCount() == 1 ? synth::kHomeRightBarColumn : synth::kContextMenuFirstColumn;
  }

  std::size_t ContextMenuColumnIndex(std::size_t column) const
  {
    return column - ContextMenuFirstColumn();
  }

  bool IsMenuKey(std::size_t row, std::size_t column) const
  {
    (void)row;
    if (synthApp_->Ui().Mode() != synth::UiMode::ContextMenu)
    {
      return false;
    }

    const std::size_t firstColumn = ContextMenuFirstColumn();
    const std::size_t columns = synthApp_->Ui().ContextMenuColumnCount();
    return column >= firstColumn && column < firstColumn + columns;
  }

  bool IsHomeRightBarKey(std::size_t column) const
  {
    return synthApp_->Ui().Mode() == synth::UiMode::Home && column == synth::kHomeRightBarColumn;
  }

  COLORREF KeyFillColor(std::size_t index, bool active) const
  {
    const auto position = PositionForIndex(index);
    if (active)
    {
      return kKeyActiveColor;
    }

    if (position.column == 0)
    {
      return synthApp_->Ui().IsModifierHighlighted(position.row) ? kModifierActiveColor : kModifierColor;
    }

    if (IsHomeRightBarKey(position.column))
    {
      return kModifierColor;
    }

    if (IsMenuKey(position.row, position.column))
    {
      return synthApp_->Ui().IsMenuCellActive(position.row, ContextMenuColumnIndex(position.column)) ? RGB(0, 76, 86) : RGB(14, 20, 24);
    }

    return kKeyColor;
  }

  COLORREF KeyEdgeColor(std::size_t index, bool active) const
  {
    const auto position = PositionForIndex(index);
    if (active)
    {
      return RGB(245, 245, 245);
    }

    if (position.column == 0 && synthApp_->Ui().IsModifierHighlighted(position.row))
    {
      return kScreenGlowColor;
    }

    if (IsHomeRightBarKey(position.column))
    {
      return kPanelEdgeColor;
    }

    if (IsMenuKey(position.row, position.column) && synthApp_->Ui().IsMenuCellActive(position.row, ContextMenuColumnIndex(position.column)))
    {
      return kScreenGlowColor;
    }

    return kPanelEdgeColor;
  }

  COLORREF KeyLedColor(std::size_t index,
                       bool deviceLedsEnabled,
                       std::uint8_t deviceLedGlobalBrightness) const
  {
    if (!deviceLedsEnabled)
    {
      return kLedOffColor;
    }

    const auto position = PositionForIndex(index);
    return BlendLedColor(Device::LED::Brightness(position.row, position.column), deviceLedGlobalBrightness);
  }

  void DrawFrameBuffer(HDC dc)
  {
    FillRectColor(dc, screenRect_, kScreenColor);

    const auto frameBuffer = Device::Display::CurrentFrameBuffer();
    if (frameBuffer.pixels == nullptr || frameBuffer.pixelCount < static_cast<std::size_t>(frameBuffer.width) * static_cast<std::size_t>(frameBuffer.height))
    {
      return;
    }

    std::vector<std::uint32_t> rgbPixels(static_cast<std::size_t>(frameBuffer.width) * static_cast<std::size_t>(frameBuffer.height));
    for (std::size_t index = 0; index < rgbPixels.size(); ++index)
    {
      const std::uint16_t pixel = frameBuffer.pixels[index];
      const std::uint8_t red = static_cast<std::uint8_t>(((pixel >> 11) & 0x1F) * 255 / 31);
      const std::uint8_t green = static_cast<std::uint8_t>(((pixel >> 5) & 0x3F) * 255 / 63);
      const std::uint8_t blue = static_cast<std::uint8_t>((pixel & 0x1F) * 255 / 31);
      rgbPixels[index] = static_cast<std::uint32_t>(blue)
                       | (static_cast<std::uint32_t>(green) << 8)
                       | (static_cast<std::uint32_t>(red) << 16);
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = frameBuffer.width;
    bitmapInfo.bmiHeader.biHeight = -frameBuffer.height;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(dc,
                  screenRect_.left,
                  screenRect_.top,
                  screenRect_.right - screenRect_.left,
                  screenRect_.bottom - screenRect_.top,
                  0,
                  0,
                  frameBuffer.width,
                  frameBuffer.height,
                  rgbPixels.data(),
                  &bitmapInfo,
                  DIB_RGB_COLORS,
                  SRCCOPY);
  }

  void DrawScaleButtons(HDC dc)
  {
    for (std::size_t index = 0; index < scaleButtons_.size(); ++index)
    {
      const int buttonScale = index == 0 ? 1 : (index == 1 ? 2 : 4);
      DrawRoundPanel(dc, scaleButtons_[index], buttonScale == scale_ ? kModifierActiveColor : kModifierColor, kPanelEdgeColor);
      RECT textRect = scaleButtons_[index];
      std::ostringstream label;
      label << buttonScale << 'x';
      DrawTextBlock(dc, textRect, label.str(), DT_CENTER | DT_SINGLELINE | DT_VCENTER, kTextColor, 16, FW_SEMIBOLD);
    }
  }

  void DrawKeyboard(HDC dc)
  {
    DrawTextBlock(dc, keyboardHeaderRect_, KeyboardHeaderText(), DT_LEFT | DT_WORDBREAK | DT_VCENTER, kTextColor, 17, FW_SEMIBOLD);
    const std::size_t activeTrack = synthApp_->Ui().ActiveTrack();
    const bool deviceLedsEnabled = Device::LED::Enabled();
    const std::uint8_t deviceLedGlobalBrightness = Device::LED::GlobalBrightness();
    for (std::size_t index = 0; index < keyButtons_.size(); ++index)
    {
      const bool active = synthApp_->IsKeyActive(activeTrack, index);
      DrawRoundPanel(dc, keyButtons_[index], KeyFillColor(index, active), KeyEdgeColor(index, active));

      RECT ledRect = MakeRect(keyButtons_[index].left + 8, keyButtons_[index].top + 7, 12, 12);
      const COLORREF ledColor = KeyLedColor(index, deviceLedsEnabled, deviceLedGlobalBrightness);
      const HBRUSH ledBrush = CreateSolidBrush(ledColor);
      const HPEN ledPen = CreatePen(PS_SOLID, 1, ledColor);
      const HGDIOBJ oldBrush = SelectObject(dc, ledBrush);
      const HGDIOBJ oldPen = SelectObject(dc, ledPen);
      Ellipse(dc, ledRect.left, ledRect.top, ledRect.right, ledRect.bottom);
      SelectObject(dc, oldBrush);
      SelectObject(dc, oldPen);
      DeleteObject(ledBrush);
      DeleteObject(ledPen);

      if (!keyboard_[index].secondaryLabel.empty())
      {
        RECT secondaryRect = keyButtons_[index];
        secondaryRect.left += 6;
        secondaryRect.right -= 6;
        secondaryRect.top += 4;
        secondaryRect.bottom = secondaryRect.top + 12;
        DrawTextBlock(dc, secondaryRect, keyboard_[index].secondaryLabel, DT_RIGHT | DT_SINGLELINE | DT_VCENTER,
                      active ? RGB(30, 41, 59) : kScreenGlowColor, 11, FW_SEMIBOLD);
      }

      RECT labelRect = keyButtons_[index];
      labelRect.top += 12;
      labelRect.bottom -= 16;
      const int labelHeight = keyboard_[index].primaryLabel.size() > 3 ? 12 : 15;
      DrawTextBlock(dc, labelRect, keyboard_[index].primaryLabel, DT_CENTER | DT_VCENTER | DT_WORDBREAK,
                    active ? RGB(15, 23, 42) : kTextColor, labelHeight, FW_SEMIBOLD);

      RECT captionRect = keyButtons_[index];
      captionRect.top = keyButtons_[index].bottom - 18;
      DrawTextBlock(dc, captionRect, keyboard_[index].note, DT_CENTER | DT_SINGLELINE | DT_VCENTER,
                    active ? RGB(30, 41, 59) : kScreenDimColor, 11);
    }
  }

  void DrawFooter(HDC dc)
  {
    DrawTextBlock(dc, footerRect_, FooterText(), DT_LEFT | DT_SINGLELINE | DT_VCENTER, kScreenDimColor, 13);
  }

  void OnPaint()
  {
    PAINTSTRUCT paint{};
    HDC windowDc = BeginPaint(hwnd_, &paint);
    RECT clientRect{};
    GetClientRect(hwnd_, &clientRect);
    UpdateLayout();

    HDC memoryDc = CreateCompatibleDC(windowDc);
    HBITMAP bitmap = CreateCompatibleBitmap(windowDc, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    const HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);

    FillRectColor(memoryDc, clientRect, kBackgroundColor);
    DrawScaleButtons(memoryDc);
    DrawFrameBuffer(memoryDc);
    DrawKeyboard(memoryDc);
    DrawFooter(memoryDc);

    BitBlt(windowDc, 0, 0, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, memoryDc, 0, 0, SRCCOPY);

    SelectObject(memoryDc, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memoryDc);
    EndPaint(hwnd_, &paint);
  }

  HWND hwnd_ = nullptr;
  int scale_ = kDefaultScale;
  int capturedKey_ = -1;
  int dragMenuIndex_ = -1;
  ULONGLONG lastFrameTick_ = 0;
  std::unique_ptr<app::Application> application_;
  apps::SynthApp* synthApp_ = nullptr;
  std::array<KeyVisual, kKeyboardRows * kKeyboardColumns> keyboard_{};
  std::array<RECT, 3> scaleButtons_{};
  std::array<RECT, kKeyboardRows * kKeyboardColumns> keyButtons_{};
  RECT screenRect_{};
  RECT keyboardHeaderRect_{};
  RECT footerRect_{};
};

}  // namespace

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int)
{
  DesktopSimulator simulator(ParseInitialScale(commandLine ? std::string(commandLine) : std::string{}));
  if (!simulator.Initialize())
  {
    return 1;
  }

  if (!simulator.Create(instance))
  {
    MessageBoxA(nullptr, "Failed to create desktop simulator window.", "ComputeCardSimDesktop", MB_ICONERROR | MB_OK);
    return 1;
  }

  return simulator.Run();
}