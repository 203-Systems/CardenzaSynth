#include "DesktopSimulator.h"

#include <sstream>
#include <vector>

#include "Applications/Synth/SynthApp.h"
#include "Devices/Device.h"
#include "DesktopHostDrawing.h"
#include "Library/Synth/Core/SynthTypes.h"

namespace ComputeCardSim::DesktopHost {

std::string DesktopSimulator::FooterText() const
{
  return "Scale: 1 / 2 / 4. Home: left edge = track/modifier bar, middle 12 keys = one octave per row, right edge = action bar. Context: hold left edge and use the two right-most columns as the menu.";
}

std::string DesktopSimulator::KeyboardHeaderText() const
{
  if (synthApp_->Ui().Mode() == synth::UiMode::ContextMenu)
  {
    return "Context mode: release the held left key to cancel, or choose a right-side item to keep this page latched.";
  }
  return "Home mode: left column switches tracks or opens modifiers, the center 12 keys perform, and the right column is reserved for home actions.";
}

std::size_t DesktopSimulator::ContextMenuFirstColumn() const
{
  return synthApp_->Ui().ContextMenuColumnCount() == 1 ? synth::kHomeRightBarColumn : synth::kContextMenuFirstColumn;
}

std::size_t DesktopSimulator::ContextMenuColumnIndex(std::size_t column) const
{
  return column - ContextMenuFirstColumn();
}

bool DesktopSimulator::IsMenuKey(std::size_t row, std::size_t column) const
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

bool DesktopSimulator::IsHomeRightBarKey(std::size_t column) const
{
  return synthApp_->Ui().Mode() == synth::UiMode::Home && column == synth::kHomeRightBarColumn;
}

COLORREF DesktopSimulator::KeyFillColor(std::size_t index, bool active) const
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

COLORREF DesktopSimulator::KeyEdgeColor(std::size_t index, bool active) const
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

COLORREF DesktopSimulator::KeyLedColor(std::size_t index,
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

void DesktopSimulator::DrawFrameBuffer(HDC dc)
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

void DesktopSimulator::DrawScaleButtons(HDC dc)
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

void DesktopSimulator::DrawKeyboard(HDC dc)
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

void DesktopSimulator::DrawFooter(HDC dc)
{
  DrawTextBlock(dc, footerRect_, FooterText(), DT_LEFT | DT_SINGLELINE | DT_VCENTER, kScreenDimColor, 13);
}

void DesktopSimulator::OnPaint()
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

}  // namespace ComputeCardSim::DesktopHost
