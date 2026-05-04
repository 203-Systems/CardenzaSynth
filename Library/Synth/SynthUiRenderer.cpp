#include "Library/Synth/SynthUiRenderer.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>

namespace synth {

namespace {

constexpr ui::Color kBackground = ui::Rgb565(0, 0, 0);
constexpr ui::Color kDivider = ui::Rgb565(136, 136, 136);
constexpr ui::Color kAccent = ui::Rgb565(0, 216, 232);
constexpr ui::Color kText = ui::Rgb565(236, 241, 246);

constexpr int kBarWidthSingle = 46;
constexpr int kBarWidthDouble = 100;
constexpr int kBarOuterPadding = 3;
constexpr int kBarCellGap = 2;
constexpr int kHomeContentLeftPadding = 6;
constexpr int kHomeContentRightPadding = 4;
constexpr int kContextContentLeftPadding = 10;
constexpr int kContextContentRightPadding = 8;
constexpr int kLoopBarCount = 4;
constexpr int kLoopBarGap = 3;

enum class BarAnchor {
  Left,
  Right,
};

struct ScreenLayout {
  ui::Rect leftBar;
  ui::Rect content;
  ui::Rect rightBar;
};

ui::Rect MakeRect(int x, int y, int width, int height)
{
  return ui::Rect{x, y, width, height};
}

float Clamp01(float value)
{
  return std::clamp(value, 0.0f, 1.0f);
}

ui::Color MixColor(ui::Color from, ui::Color to, float t)
{
  t = Clamp01(t);

  const int fromRed = ((from >> 11) & 0x1F) * 255 / 31;
  const int fromGreen = ((from >> 5) & 0x3F) * 255 / 63;
  const int fromBlue = (from & 0x1F) * 255 / 31;

  const int toRed = ((to >> 11) & 0x1F) * 255 / 31;
  const int toGreen = ((to >> 5) & 0x3F) * 255 / 63;
  const int toBlue = (to & 0x1F) * 255 / 31;

  const auto lerp = [t](int a, int b) {
    return static_cast<int>(static_cast<float>(a) + static_cast<float>(b - a) * t + 0.5f);
  };

  return ui::Rgb565(static_cast<std::uint8_t>(lerp(fromRed, toRed)),
                    static_cast<std::uint8_t>(lerp(fromGreen, toGreen)),
                    static_cast<std::uint8_t>(lerp(fromBlue, toBlue)));
}

ui::Color FadeColor(ui::Color color, float alpha)
{
  return MixColor(kBackground, color, alpha);
}

ui::Rect LerpRect(const ui::Rect& from, const ui::Rect& to, float t)
{
  t = Clamp01(t);
  const auto lerp = [t](int a, int b) {
    return static_cast<int>(static_cast<float>(a) + static_cast<float>(b - a) * t + 0.5f);
  };

  return MakeRect(lerp(from.x, to.x), lerp(from.y, to.y), lerp(from.width, to.width), lerp(from.height, to.height));
}

int BarWidthForColumns(std::size_t columns, float widthScale)
{
  const int baseWidth = columns == 1 ? kBarWidthSingle : kBarWidthDouble;
  const float normalizedScale = std::max(1.0f, widthScale);
  return std::max(baseWidth, static_cast<int>(static_cast<float>(baseWidth) * normalizedScale + 0.5f));
}

void DrawHomeTrackLabel(ui::Canvas& canvas, const ui::Rect& rect, const std::string& trackLabel, float alpha)
{
  if (trackLabel.empty() || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const std::size_t separator = trackLabel.find_last_of(' ');
  if (separator == std::string::npos || separator + 1 >= trackLabel.size())
  {
    canvas.DrawTextInRect(rect, trackLabel, text, 1, ui::TextAlign::Right, ui::VerticalAlign::Top);
    return;
  }

  const std::string label = trackLabel.substr(0, separator + 1);
  const std::string number = trackLabel.substr(separator + 1);
  const int numberWidth = canvas.MeasureTextWidth(number, 1);
  const int labelWidth = canvas.MeasureTextWidth(label, 1);
  const int rightEdge = rect.x + rect.width;
  const int labelX = std::max(rect.x, rightEdge - numberWidth - labelWidth);
  const int numberX = std::max(rect.x, rightEdge - numberWidth);

  canvas.DrawText(labelX, rect.y, label, text, 1);
  canvas.DrawText(numberX, rect.y, number, accent, 1);
}

ScreenLayout LayoutForSnapshot(const ui::Canvas& canvas, const UiScreenSnapshot& snapshot)
{
  ScreenLayout layout;
  const int leftWidth = BarWidthForColumns(snapshot.leftBarColumns, snapshot.leftBarWidthScale);
  layout.leftBar = MakeRect(snapshot.leftBarVisible ? 0 : -leftWidth, 0, leftWidth, canvas.Height());

  const int rightWidth = snapshot.rightBarVisible ? BarWidthForColumns(snapshot.rightBarColumns, snapshot.rightBarWidthScale) : 0;
  layout.rightBar = MakeRect(canvas.Width() - rightWidth, 0, rightWidth, canvas.Height());

  const int contentLeftPadding = snapshot.mode == UiMode::Home ? kHomeContentLeftPadding : kContextContentLeftPadding;
  const int contentRightPadding = snapshot.mode == UiMode::Home ? kHomeContentRightPadding : kContextContentRightPadding;
  const int contentLeft = (snapshot.leftBarVisible ? leftWidth : 0) + contentLeftPadding;
  const int contentRight = layout.rightBar.x - contentRightPadding;
  layout.content = MakeRect(contentLeft, 0, std::max(0, contentRight - contentLeft), canvas.Height());
  return layout;
}

std::string TruncateText(const ui::Canvas& canvas, std::string value, int maxWidth, int scale)
{
  if (canvas.MeasureTextWidth(value, scale) <= maxWidth)
  {
    return value;
  }

  while (value.size() > 3 && canvas.MeasureTextWidth(value + "...", scale) > maxWidth)
  {
    value.pop_back();
  }
  return value + "...";
}

void DrawFilledCircle(ui::Canvas& canvas, int centerX, int centerY, int radius, ui::Color color)
{
  if (radius <= 0)
  {
    return;
  }

  int x = radius;
  int y = 0;
  int error = 1 - x;

  while (x >= y)
  {
    canvas.DrawLine(centerX - x, centerY + y, centerX + x, centerY + y, color);
    canvas.DrawLine(centerX - y, centerY + x, centerX + y, centerY + x, color);
    canvas.DrawLine(centerX - x, centerY - y, centerX + x, centerY - y, color);
    canvas.DrawLine(centerX - y, centerY - x, centerX + y, centerY - x, color);

    ++y;
    if (error < 0)
    {
      error += 2 * y + 1;
    }
    else
    {
      --x;
      error += 2 * (y - x) + 1;
    }
  }
}

void DrawBarIcon(ui::Canvas& canvas, const ui::Rect& rect, UiBarIcon icon, ui::Color color)
{
  if (icon == UiBarIcon::None || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int centerX = rect.x + rect.width / 2;
  const int centerY = rect.y + rect.height / 2;
  const int iconRadius = std::max(3, std::min(rect.width, rect.height) / 4);
  const int halfHeight = std::max(3, rect.height / 4);

  switch (icon)
  {
    case UiBarIcon::Plus:
      canvas.DrawLine(centerX - iconRadius, centerY, centerX + iconRadius, centerY, color);
      canvas.DrawLine(centerX, centerY - iconRadius, centerX, centerY + iconRadius, color);
      break;
    case UiBarIcon::Minus:
      canvas.DrawLine(centerX - iconRadius, centerY, centerX + iconRadius, centerY, color);
      break;
    case UiBarIcon::Circle:
      DrawFilledCircle(canvas, centerX, centerY, std::max(5, iconRadius), color);
      break;
    case UiBarIcon::Play:
    {
      const int leftX = centerX - std::max(3, rect.width / 6);
      const int rightX = centerX + std::max(3, rect.width / 6);
      for (int y = -halfHeight; y <= halfHeight; ++y)
      {
        const int horizontalExtent = static_cast<int>(static_cast<float>(rightX - leftX) *
                                                      (1.0f - static_cast<float>(std::abs(y)) / std::max(1, halfHeight)) +
                                                      0.5f);
        canvas.DrawLine(leftX, centerY + y, leftX + horizontalExtent, centerY + y, color);
      }
      break;
    }
    case UiBarIcon::None:
    default:
      break;
  }
}

void DrawBarItem(ui::Canvas& canvas, const ui::Rect& rect, const UiBarItemSnapshot& item, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0 ||
      (item.label.empty() && item.icon == UiBarIcon::None))
  {
    return;
  }

  const ui::Color border = FadeColor(item.active ? kAccent : kDivider, alpha);
  const ui::Color text = FadeColor(item.active ? kAccent : kText, alpha);

  canvas.StrokeRect(rect, border);

  if (item.icon != UiBarIcon::None)
  {
    DrawBarIcon(canvas, MakeRect(rect.x + 4, rect.y + 4, std::max(0, rect.width - 8), std::max(0, rect.height - 8)), item.icon, text);
    return;
  }

  const ui::Rect textRect = MakeRect(rect.x + 2, rect.y + 1, std::max(0, rect.width - 4), std::max(0, rect.height - 2));

  canvas.DrawTextInRect(textRect,
                        TruncateText(canvas, item.label, textRect.width, 1),
                        text,
                        1,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);
}

void DrawSegmentedBar(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      std::size_t columns,
                      const UiBarSnapshot& items,
                      BarAnchor anchor,
                      float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0 || columns == 0)
  {
    return;
  }

  (void)anchor;

  const int innerX = rect.x + kBarOuterPadding;
  const int innerY = rect.y + kBarOuterPadding;
  const int innerWidth = std::max(0, rect.width - kBarOuterPadding * 2);
  const int innerHeight = std::max(0, rect.height - kBarOuterPadding * 2);
  const int cellWidth = std::max(0,
                                 (innerWidth - static_cast<int>(columns - 1) * kBarCellGap) /
                                     static_cast<int>(columns));
  const int cellHeight = std::max(0,
                                  (innerHeight - static_cast<int>(kContextMenuRows - 1) * kBarCellGap) /
                                      static_cast<int>(kContextMenuRows));

  for (std::size_t row = 0; row < kContextMenuRows; ++row)
  {
    const int y0 = innerY + static_cast<int>(row) * (cellHeight + kBarCellGap);
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + column;
      const int x0 = innerX + static_cast<int>(column) * (cellWidth + kBarCellGap);
      DrawBarItem(canvas, MakeRect(x0, y0, cellWidth, cellHeight), items[index], alpha);
    }
  }
}

void DrawMiniKeyboard(ui::Canvas& canvas,
                      const ui::Rect& rect,
                      const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                      float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);

  canvas.StrokeRect(rect, divider);

  constexpr int kWhiteKeyCount = 10;
  for (int index = 1; index < kWhiteKeyCount; ++index)
  {
    const int x = rect.x + index * rect.width / kWhiteKeyCount;
    canvas.DrawLine(x, rect.y, x, rect.y + rect.height - 1, divider);
  }

  const int blackKeyHeight = rect.height * 3 / 5;
  const int blackKeyWidth = std::max(4, rect.width / 28);
  constexpr std::array<int, 7> kBlackKeySlots = {1, 2, 4, 5, 6, 8, 9};
  for (int slot : kBlackKeySlots)
  {
    const int centerX = rect.x + slot * rect.width / kWhiteKeyCount;
    canvas.StrokeRect(MakeRect(centerX - blackKeyWidth / 2, rect.y, blackKeyWidth, blackKeyHeight), divider);
  }

  for (std::size_t index = 0; index < activeKeys.size(); ++index)
  {
    if (!activeKeys[index])
    {
      continue;
    }

    const int x = rect.x + 2 + static_cast<int>(index) * std::max(1, rect.width - 4) / static_cast<int>(activeKeys.size());
    canvas.DrawLine(x, rect.y + rect.height - 4, x + 3, rect.y + rect.height - 4, accent);
  }
}

void DrawThinLooperBar(ui::Canvas& canvas, const ui::Rect& rect, float progress, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const int segmentWidth = std::max(0, (rect.width - (kLoopBarCount - 1) * kLoopBarGap) / kLoopBarCount);

  for (int index = 0; index < kLoopBarCount; ++index)
  {
    const int x = rect.x + index * (segmentWidth + kLoopBarGap);
    const ui::Rect segmentRect = MakeRect(x, rect.y, segmentWidth, rect.height);
    canvas.FillRect(segmentRect, divider);

    const float segmentProgress = Clamp01(progress * static_cast<float>(kLoopBarCount) - static_cast<float>(index));
    const int fillWidth = std::clamp(static_cast<int>(static_cast<float>(segmentRect.width) * segmentProgress), 0, segmentRect.width);
    if (fillWidth > 0)
    {
      canvas.FillRect(MakeRect(segmentRect.x, segmentRect.y, fillWidth, segmentRect.height), accent);
    }
  }
}

void DrawHomeContent(ui::Canvas& canvas,
                     const ui::Rect& rect,
                     const UiScreenSnapshot& snapshot,
                     const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                     float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  canvas.DrawText(rect.x + 4, rect.y + 8, TruncateText(canvas, snapshot.headerPrimary, rect.width - 92, 1), accent, 1);
  DrawHomeTrackLabel(canvas, MakeRect(rect.x + rect.width - 86, rect.y + 8, 82, 10), snapshot.headerSecondary, alpha);
  canvas.DrawLine(rect.x + 4, rect.y + 24, rect.x + rect.width - 5, rect.y + 24, divider);
  canvas.DrawTextInRect(MakeRect(rect.x + 4, rect.y + 34, rect.width - 8, 34),
                        TruncateText(canvas, snapshot.primaryText, rect.width - 12, 3),
                        text,
                        3,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);

  DrawMiniKeyboard(canvas, MakeRect(rect.x + 16, rect.y + 74, rect.width - 32, 28), activeKeys, alpha);
  DrawThinLooperBar(canvas, MakeRect(rect.x + 4, rect.y + rect.height - 11, rect.width - 8, 4), snapshot.looperProgress, alpha);
}

void DrawContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  canvas.DrawText(rect.x + 10, rect.y + 10, TruncateText(canvas, snapshot.headerPrimary, rect.width - 20, 1), accent, 1);

  if (!snapshot.showSecondaryText)
  {
    canvas.DrawTextInRect(MakeRect(rect.x + 10, rect.y + 34, rect.width - 20, 34), snapshot.primaryText, text, 3,
                          ui::TextAlign::Left, ui::VerticalAlign::Middle);
    return;
  }

  canvas.DrawTextInRect(MakeRect(rect.x + 10, rect.y + 30, rect.width - 20, 24),
                        TruncateText(canvas, snapshot.primaryText, rect.width - 20, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x + 10, rect.y + 58, rect.x + rect.width - 26, rect.y + 58, divider);
  canvas.DrawTextInRect(MakeRect(rect.x + 10, rect.y + 64, rect.width - 20, 42),
                        TruncateText(canvas, snapshot.secondaryText, rect.width - 20, 5),
                        text,
                        5,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

void DrawSnapshotContent(ui::Canvas& canvas,
                         const ui::Rect& rect,
                         const UiScreenSnapshot& snapshot,
                         const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                         float alpha)
{
  if (snapshot.mode == UiMode::Home)
  {
    DrawHomeContent(canvas, rect, snapshot, activeKeys, alpha);
    return;
  }

  DrawContextContent(canvas, rect, snapshot, alpha);
}

void DrawSnapshotFrame(ui::Canvas& canvas,
                       const UiScreenSnapshot& snapshot,
                       const ScreenLayout& layout,
                       const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                       float alpha)
{
  DrawSegmentedBar(canvas, layout.leftBar, snapshot.leftBarColumns, snapshot.leftBarItems, BarAnchor::Left, alpha);
  DrawSegmentedBar(canvas, layout.rightBar, snapshot.rightBarColumns, snapshot.rightBarItems, BarAnchor::Right, alpha);
  DrawSnapshotContent(canvas, layout.content, snapshot, activeKeys, alpha);
}

void DrawTransitionFrame(ui::Canvas& canvas,
                         const UiTransitionSnapshot& transition,
                         const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys)
{
  const float progress = Clamp01(transition.progress);
  const ScreenLayout fromLayout = LayoutForSnapshot(canvas, transition.from);
  const ScreenLayout toLayout = LayoutForSnapshot(canvas, transition.to);

  ScreenLayout blendedLayout;
  blendedLayout.leftBar = LerpRect(fromLayout.leftBar, toLayout.leftBar, progress);
  blendedLayout.content = LerpRect(fromLayout.content, toLayout.content, progress);
  blendedLayout.rightBar = LerpRect(fromLayout.rightBar, toLayout.rightBar, progress);

  DrawSnapshotFrame(canvas, transition.from, blendedLayout, activeKeys, 1.0f - progress);
  DrawSnapshotFrame(canvas, transition.to, blendedLayout, activeKeys, progress);
}

}  // namespace

void RenderSynthUiFrame(ui::Canvas& canvas,
                        const SynthUiModel& model,
                        const ISynthEngine* engine,
                        const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys)
{
  canvas.Clear(kBackground);

  if (model.Transition().active)
  {
    DrawTransitionFrame(canvas, model.Transition(), activeKeys);
    return;
  }

  const UiScreenSnapshot snapshot = model.CurrentScreenSnapshot(engine);
  DrawSnapshotFrame(canvas, snapshot, LayoutForSnapshot(canvas, snapshot), activeKeys, 1.0f);
}

}  // namespace synth
