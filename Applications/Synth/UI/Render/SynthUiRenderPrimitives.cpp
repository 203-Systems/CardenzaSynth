#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>

namespace synth::ui_render {

namespace {
std::uint32_t gMarqueeFrame = 0;
}  // namespace

void AdvanceMarqueeFrame()
{
  ++gMarqueeFrame;
}

ui::Rect MakeRect(int x, int y, int width, int height)
{
  return ui::Rect{x, y, width, height};
}

ui::Rect InsetRect(const ui::Rect& rect, int insetX, int insetY)
{
  return MakeRect(rect.x + insetX,
                  rect.y + insetY,
                  std::max(0, rect.width - insetX * 2),
                  std::max(0, rect.height - insetY * 2));
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

void DrawMarqueeTextInRect(ui::Canvas& canvas,
                           const ui::Rect& rect,
                           std::string_view text,
                           ui::Color color,
                           int scale,
                           ui::TextAlign fitAlign)
{
  if (rect.width <= 0 || rect.height <= 0 || text.empty())
  {
    return;
  }

  const int textWidth = canvas.MeasureTextWidth(text, scale);
  if (textWidth <= rect.width)
  {
    canvas.DrawTextInRect(rect, text, color, scale, fitAlign, ui::VerticalAlign::Middle);
    return;
  }

  const int gapWidth = std::max(kMarqueeGapPixels, canvas.MeasureTextWidth("   ", scale));
  const std::uint32_t steppedFrame = gMarqueeFrame / kMarqueeStepFrames;
  const int cycleWidth = textWidth + gapWidth;
  const int phase = static_cast<int>(steppedFrame % static_cast<std::uint32_t>(kMarqueePauseFrames + cycleWidth));
  const int offset = phase < kMarqueePauseFrames ? 0 : phase - kMarqueePauseFrames;
  const int textY = rect.y + (rect.height - canvas.FontHeight(scale)) / 2;

  canvas.DrawTextClipped(rect, rect.x - offset, textY, text, color, scale);
  canvas.DrawTextClipped(rect, rect.x - offset + cycleWidth, textY, text, color, scale);
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

  const ui::Color border = FadeColor(item.disabled ? MixColor(kBackground, kDivider, 0.55f)
                                                   : (item.active ? kAccent : kDivider),
                                     alpha);
  const ui::Color text = FadeColor(item.disabled ? kDivider : (item.active ? kAccent : kText), alpha);

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
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color background = FadeColor(kBackground, alpha);
  std::array<bool, 12> activeNotes{};

  for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
  {
    for (std::size_t column = kHomePerformanceFirstColumn; column <= kHomePerformanceLastColumn; ++column)
    {
      const std::size_t keyIndex = row * kKeyboardMatrixColumns + column;
      if (!activeKeys[keyIndex])
      {
        continue;
      }

      activeNotes[column - kHomePerformanceFirstColumn] = true;
    }
  }

  constexpr std::array<std::size_t, 7> kWhiteSemitones = {0, 2, 4, 5, 7, 9, 11};
  constexpr std::array<std::size_t, 5> kBlackSemitones = {1, 3, 6, 8, 10};
  constexpr std::array<std::size_t, 5> kBlackKeyLeftWhite = {0, 1, 3, 4, 5};
  std::array<int, kWhiteSemitones.size() + 1> whiteBoundaries{};
  std::array<ui::Rect, kWhiteSemitones.size()> whiteKeys{};

  canvas.FillRect(InsetRect(rect, 1, 1), background);
  canvas.StrokeRect(rect, MixColor(divider, text, 0.35f));

  const int keybedPadding = std::max(3, rect.height / 8);
  const ui::Rect keybed = InsetRect(rect, keybedPadding, keybedPadding);
  if (keybed.width <= 0 || keybed.height <= 0)
  {
    return;
  }

  const int nominalWhiteWidth = std::max(1, keybed.width / static_cast<int>(kWhiteSemitones.size()));

  for (std::size_t index = 0; index <= kWhiteSemitones.size(); ++index)
  {
    whiteBoundaries[index] = keybed.x + static_cast<int>(index) * keybed.width / static_cast<int>(kWhiteSemitones.size());
  }

  for (std::size_t index = 0; index < kWhiteSemitones.size(); ++index)
  {
    int left = whiteBoundaries[index];
    int right = whiteBoundaries[index + 1];
    if (index > 0)
    {
      ++left;
    }
    if (index + 1 < kWhiteSemitones.size())
    {
      --right;
    }

    whiteKeys[index] = MakeRect(left, keybed.y, std::max(1, right - left), keybed.height);
    canvas.FillRect(whiteKeys[index], activeNotes[kWhiteSemitones[index]] ? accent : text);
  }

  const int blackKeyHeight = std::max(4, keybed.height * 3 / 5);
  for (std::size_t index = 0; index < kBlackSemitones.size(); ++index)
  {
    int blackKeyWidth = std::max(5, nominalWhiteWidth * 11 / 20);

    const int seamX = whiteBoundaries[kBlackKeyLeftWhite[index] + 1];
    const int left = std::clamp(seamX - blackKeyWidth / 2,
                                keybed.x + 1,
                                keybed.x + keybed.width - blackKeyWidth - 1);
    const ui::Rect blackKey = MakeRect(left, keybed.y, blackKeyWidth, blackKeyHeight);
    const bool isActive = activeNotes[kBlackSemitones[index]];
    canvas.FillRect(blackKey, background);
    canvas.StrokeRect(blackKey, background);
    if (isActive)
    {
      canvas.FillRect(MakeRect(blackKey.x + 1,
                               blackKey.y,
                               std::max(0, blackKey.width - 2),
                               std::max(0, blackKey.height - 1)),
                      accent);
    }
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

}  // namespace synth::ui_render
