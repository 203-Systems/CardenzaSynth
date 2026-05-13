#include "DesktopSimulator.h"

#include "Devices/ComputeCardSim/SimulatorInput.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "OS/System/System.h"

namespace ComputeCardSim::DesktopHost {

void DesktopSimulator::OnLeftButtonDown(int x, int y)
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

  if (PointInRect(homeButton_, point))
  {
    TapHomeButton();
    InvalidateRect(hwnd_, nullptr, FALSE);
    return;
  }

  for (std::size_t index = 0; index < keyButtons_.size(); ++index)
  {
    if (PointInRect(keyButtons_[index], point))
    {
      const auto position = PositionForIndex(index);
      const bool shouldCapture = ShouldCaptureKey(position);
      SetVirtualKey(position, true);
      if (shouldCapture)
      {
        SetCapture(hwnd_);
        capturedKey_ = static_cast<int>(index);
      }
      else
      {
        SetVirtualKey(position, false);
      }
      InvalidateRect(hwnd_, nullptr, FALSE);
      return;
    }
  }
}

void DesktopSimulator::OnMouseMove(int x, int y, UINT flags)
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

    TapVirtualKey(position);
    dragMenuIndex_ = static_cast<int>(index);
    InvalidateRect(hwnd_, nullptr, FALSE);
    return;
  }

  dragMenuIndex_ = -1;
}

void DesktopSimulator::OnLeftButtonUp()
{
  if (capturedKey_ < 0)
  {
    return;
  }

  const auto position = PositionForIndex(static_cast<std::size_t>(capturedKey_));
  SetVirtualKey(position, false);

  capturedKey_ = -1;
  dragMenuIndex_ = -1;
  ReleaseCapture();
  InvalidateRect(hwnd_, nullptr, FALSE);
}

void DesktopSimulator::OnKeyDown(UINT virtualKey)
{
  if (virtualKey == '1' || virtualKey == '2' || virtualKey == '4')
  {
    const int nextScale = virtualKey == '1' ? 1 : (virtualKey == '2' ? 2 : 4);
    if (nextScale != scale_)
    {
      scale_ = nextScale;
      ResizeForScale();
    }
    return;
  }

  if (virtualKey == VK_HOME)
  {
    TapHomeButton();
    InvalidateRect(hwnd_, nullptr, FALSE);
  }
}

void DesktopSimulator::TapHomeButton()
{
  ComputeCardSim::Input::SetHomeButton(true);
  os::system::RunFrame();
  ComputeCardSim::Input::SetHomeButton(false);
  os::system::RunFrame();
}

bool DesktopSimulator::ShouldCaptureKey(MatrixPosition position) const
{
  return position.column == synth::kHomeLeftBarColumn ||
         position.column == synth::kHomeRightBarColumn ||
         IsMenuKey(position.row, position.column) ||
         (position.column >= synth::kHomePerformanceFirstColumn &&
          position.column <= synth::kHomePerformanceLastColumn);
}

void DesktopSimulator::SetVirtualKey(MatrixPosition position, bool pressed)
{
  ComputeCardSim::Input::SetMatrixKey(position.row, position.column, pressed);
  os::system::RunFrame();
}

void DesktopSimulator::TapVirtualKey(MatrixPosition position)
{
  SetVirtualKey(position, true);
  SetVirtualKey(position, false);
}

}  // namespace ComputeCardSim::DesktopHost
