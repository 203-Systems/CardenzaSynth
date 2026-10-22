#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace synth::ui_render {

namespace {
std::uint32_t gMarqueeFrame = 0;

int PixelScale(int scale)
{
  return scale <= 1 ? 1 : (scale <= 3 ? 2 : 3);
}

int InkTopOffset(const ui::Canvas& canvas, int scale)
{
  return (canvas.FontHeight(scale) - InkHeight(scale)) / 2;
}

bool HasBarContent(const UiBarItemSnapshot& item)
{
  return !item.label.empty() || item.icon != UiBarIcon::None;
}

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
  const auto newline = value.find('\n');
  if (newline != std::string::npos)
  {
    return TruncateText(canvas, value.substr(0, newline), maxWidth, scale) + "\n" +
           TruncateText(canvas, value.substr(newline + 1), maxWidth, scale);
  }
  if (canvas.MeasureTextWidth(value, scale) <= maxWidth)
  {
    return value;
  }

  std::string suffix = "...";
  while (!suffix.empty() && canvas.MeasureTextWidth(suffix, scale) > maxWidth) suffix.pop_back();
  while (!value.empty() && canvas.MeasureTextWidth(value + suffix, scale) > maxWidth)
  {
    value.pop_back();
  }
  return value + suffix;
}

std::string NoteName(int midiNote)
{
  static constexpr std::array<const char*, 12> kNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int note = std::clamp(midiNote, 0, 127);
  return std::string(kNames[static_cast<std::size_t>(note % 12)]) + std::to_string(note / 12 - 1);
}

int InkHeight(int scale)
{
  return scale <= 1 ? 9 : 7 * PixelScale(scale);
}

int InkWidth(const ui::Canvas& canvas, std::string_view text, int scale)
{
  return text.empty() ? 0 : canvas.MeasureTextWidth(text, scale) - PixelScale(scale);
}

void DrawInk(ui::Canvas& canvas, int x, int top, std::string_view text, ui::Color color, int scale)
{
  canvas.DrawText(x, top - InkTopOffset(canvas, scale), text, color, scale);
}

void DrawInkClipped(ui::Canvas& canvas, const ui::Rect& clip, int x, int top, std::string_view text, ui::Color color, int scale)
{
  canvas.DrawTextClipped(clip, x, top - InkTopOffset(canvas, scale), text, color, scale);
}

int FitScale(const ui::Canvas& canvas, std::string_view text, int maxWidth, int maxScale)
{
  for (int scale : {4, 2, 1})
  {
    if (scale <= maxScale && InkWidth(canvas, text, scale) <= maxWidth)
    {
      return scale;
    }
  }
  return 1;
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

  const int textWidth = InkWidth(canvas, text, scale);
  const int inkTop = rect.y + (rect.height - InkHeight(scale)) / 2;
  if (textWidth <= rect.width)
  {
    const int x = fitAlign == ui::TextAlign::Right ? rect.x + rect.width - textWidth
                : fitAlign == ui::TextAlign::Center ? rect.x + (rect.width - textWidth) / 2
                                                     : rect.x;
    DrawInk(canvas, x, inkTop, text, color, scale);
    return;
  }

  const int gapWidth = std::max(kMarqueeGapPixels, canvas.MeasureTextWidth("   ", scale));
  const std::uint32_t steppedFrame = gMarqueeFrame / kMarqueeStepFrames;
  const int cycleWidth = textWidth + gapWidth;
  const int phase = static_cast<int>(steppedFrame % static_cast<std::uint32_t>(kMarqueePauseFrames + cycleWidth));
  const int offset = phase < kMarqueePauseFrames ? 0 : phase - kMarqueePauseFrames;

  DrawInkClipped(canvas, rect, rect.x - offset, inkTop, text, color, scale);
  DrawInkClipped(canvas, rect, rect.x - offset + cycleWidth, inkTop, text, color, scale);
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

void DrawFrame(ui::Canvas& canvas, const ui::Rect& rect, ui::Color color, int thickness)
{
  for (int inset = 0; inset < thickness && rect.width > inset * 2 && rect.height > inset * 2; ++inset)
    canvas.StrokeRect(InsetRect(rect, inset, inset), color);
}

void DrawBarIcon(ui::Canvas& canvas, const ui::Rect& rect, UiBarIcon icon, ui::Color color)
{
  if (icon == UiBarIcon::None || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  constexpr int kSize = 11;
  const int left = rect.x + (rect.width - kSize) / 2;
  const int top = rect.y + (rect.height - kSize) / 2;
  const int center = kSize / 2;

  switch (icon)
  {
    case UiBarIcon::Plus:
      canvas.FillRect(MakeRect(left, top + center - 1, kSize, 2), color);
      canvas.FillRect(MakeRect(left + center - 1, top, 2, kSize), color);
      break;
    case UiBarIcon::Minus:
      canvas.FillRect(MakeRect(left, top + center - 1, kSize, 2), color);
      break;
    case UiBarIcon::Circle:
      DrawFilledCircle(canvas, left + center, top + center, center, color);
      break;
    case UiBarIcon::Stop:
      canvas.FillRect(MakeRect(left + 1, top + 1, 9, 9), color);
      break;
    case UiBarIcon::Play:
    {
      constexpr std::array<int, 10> kExtents = {5, 5, 4, 4, 3, 3, 2, 2, 1, 0};
      for (std::size_t column = 0; column < kExtents.size(); ++column)
      {
        const int extent = kExtents[column];
        canvas.FillRect(MakeRect(left + static_cast<int>(column), top + center - extent, 1, extent * 2 + 1), color);
      }
      break;
    }
    case UiBarIcon::None:
    default:
      break;
  }
}

ui::Rect BarTileRect(const ui::Rect& bar, std::size_t columns, std::size_t row, std::size_t column)
{
  columns = std::max<std::size_t>(1, columns);
  const int rows = static_cast<int>(kContextMenuRows);
  const ui::Rect inner = InsetRect(bar, kBarTilePadding, kBarTilePadding);
  const int width = (inner.width - kBarTileGap * static_cast<int>(columns - 1)) / static_cast<int>(columns);
  const int height = (inner.height - kBarTileGap * (rows - 1)) / rows;
  return MakeRect(inner.x + static_cast<int>(column) * (width + kBarTileGap), inner.y + static_cast<int>(row) * (height + kBarTileGap),
                  width, height);
}

void DrawBarItem(ui::Canvas& canvas, const ui::Rect& rect, const UiBarItemSnapshot& item, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }
  if (!HasBarContent(item))
  {
    // Unused keys keep a faint outline so the column always reads as four keys.
    canvas.StrokeRect(rect, FadeColor(MixColor(kBackground, kRule, 0.55f), alpha));
    return;
  }

  // Held keys use white ink on teal; latched choices use a pale teal plate.
  canvas.FillRect(rect, FadeColor(kSurface, alpha));
  DrawFrame(canvas, rect, FadeColor(kRule, alpha));
  ui::Color ink = item.danger ? kDanger : kText;
  if (item.disabled)
  {
    ink = MixColor(kRule, kDim, 0.45f);
  }
  else if (item.pressed)
  {
    canvas.FillRect(rect, FadeColor(item.danger ? kDanger : kAccent, alpha));
    ink = kInverseText;
  }
  else if (item.active)
  {
    canvas.FillRect(rect, FadeColor(kSelection, alpha));
    DrawFrame(canvas, rect, FadeColor(item.danger ? kDanger : kAccent, alpha));
    ink = item.danger ? kDanger : kAccent;
  }
  else if (item.playing)
  {
    ink = kPlayback;
  }
  ink = FadeColor(ink, alpha);

  if (item.icon != UiBarIcon::None)
  {
    DrawBarIcon(canvas, rect, item.icon, ink);
    return;
  }

  // Wider tiles reserve two pixels around six readable 6x9 characters.
  const std::string label = TruncateText(canvas, item.label, rect.width - 4, 1);
  const int width = InkWidth(canvas, label, 1);
  DrawInk(canvas, rect.x + (rect.width - width + 1) / 2, rect.y + (rect.height - InkHeight(1)) / 2, label, ink, 1);
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
  for (std::size_t row = 0; row < kContextMenuRows; ++row)
  {
    for (std::size_t column = 0; column < columns; ++column)
    {
      const std::size_t index = columns == 1 ? row : row * kContextMenuColumns + column;
      DrawBarItem(canvas, BarTileRect(rect, columns, row, column), items[index], alpha);
    }
  }
}

void DrawTitleBar(ui::Canvas& canvas,
                  std::string_view title,
                  std::string_view line1,
                  ui::Color line1Color,
                  std::string_view line2,
                  std::string_view status,
                  ui::Color statusColor,
                  float alpha)
{
  const int width = canvas.Width();
  canvas.FillRect(MakeRect(0, 0, width, kTitleBarHeight), FadeColor(kHeader, alpha));
  canvas.FillRect(MakeRect(0, kTitleBarHeight - 2, width, 2), FadeColor(kRule, alpha));
  const int top = (kTitleBarHeight - InkHeight(2)) / 2;
  const int line1Top = line2.empty() ? (kTitleBarHeight - InkHeight(1) - 2) / 2 : 2;
  const int line2Top = 12;
  const std::string statusText = TruncateText(canvas, std::string(status), width / 3, 1);
  const int statusX = width - kTitleBarInset - InkWidth(canvas, statusText, 1);
  DrawInk(canvas, statusX, line1Top, statusText, FadeColor(statusColor, alpha), 1);
  const int right = statusText.empty() ? width - kTitleBarInset : statusX - 10;

  // A trailing index joins its word, as in TRACK1 or OPERATOR1.
  std::string name(title);
  const std::size_t space = name.find_last_of(' ');
  if (space != std::string::npos && space + 1 < name.size() && name.find_first_not_of("0123456789", space + 1) == std::string::npos)
  {
    name.erase(space, 1);
  }
  name = TruncateText(canvas, name, right - kTitleBarInset, 2);
  DrawInk(canvas, kTitleBarInset, top, name, FadeColor(kText, alpha), 2);

  const int smallX = kTitleBarInset + InkWidth(canvas, name, 2) + 6;
  const int smallWidth = std::max(0, right - smallX);
  DrawMarqueeTextInRect(canvas, MakeRect(smallX, line1Top, smallWidth, InkHeight(1)), line1, FadeColor(line1Color, alpha), 1);
  DrawMarqueeTextInRect(canvas, MakeRect(smallX, line2Top, smallWidth, InkHeight(1)), line2, FadeColor(kDim, alpha), 1);
}

int DrawPageTitle(ui::Canvas& canvas,
                  const ui::Rect& rect,
                  std::string_view title,
                  std::string_view line1,
                  ui::Color line1Color,
                  std::string_view line2,
                  std::string_view status,
                  float alpha)
{
  DrawTitleBar(canvas, title, line1, line1Color, line2, status, kDim, alpha);
  return std::max(rect.y, kTitleBarHeight) + 4;
}

int DrawPageHeader(ui::Canvas& canvas,
                   const ui::Rect& rect,
                   std::string_view title,
                   std::string_view meta,
                   ui::Color titleColor,
                   ui::Color metaColor,
                   float alpha)
{
  (void)titleColor;
  const ui::Color line1Color = metaColor == kDanger || metaColor == kPlayback ? metaColor : kAccent;
  return DrawPageTitle(canvas, rect, title, meta, line1Color, {}, {}, alpha);
}

void DrawMeter(ui::Canvas& canvas, const ui::Rect& rect, float level, ui::Color fill, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0 || level < 0.0f)
  {
    return;
  }
  canvas.FillRect(rect, FadeColor(kRule, alpha));
  const int fillWidth = std::clamp(static_cast<int>(static_cast<float>(rect.width) * Clamp01(level) + 0.5f), 0, rect.width);
  if (fillWidth > 0)
  {
    canvas.FillRect(MakeRect(rect.x, rect.y, fillWidth, rect.height), FadeColor(fill, alpha));
  }
}

ui::Rect KeyValueEntryRect(const ui::Rect& content, int canvasHeight, std::size_t columns, std::size_t row, std::size_t column)
{
  constexpr int kColumnGap = 10;
  columns = std::max<std::size_t>(1, columns);
  const ui::Rect tile = BarTileRect(MakeRect(0, kTitleBarHeight, 1, canvasHeight - kTitleBarHeight), 1, row, 0);
  const int width = (content.width - kColumnGap * static_cast<int>(columns - 1)) / static_cast<int>(columns);
  return MakeRect(content.x + static_cast<int>(column) * (width + kColumnGap), tile.y, width, tile.height);
}

void DrawKeyValues(ui::Canvas& canvas,
                       const ui::Rect& content,
                       std::size_t columns,
                       const UiBarSnapshot& items,
                       float alpha)
{
  const auto itemAt = [&](std::size_t row, std::size_t column) -> const UiBarItemSnapshot& {
    return items[columns == 1 ? row : row * kContextMenuColumns + column];
  };
  for (std::size_t column = 0; column < columns; ++column)
  {
    // Values in one column start at a shared x so the numbers line up like a table.
    int labelWidth = 0;
    for (std::size_t row = 0; row < kContextMenuRows; ++row)
    {
      if (!itemAt(row, column).value.empty()) labelWidth = std::max(labelWidth, InkWidth(canvas, itemAt(row, column).label, 1));
    }

    for (std::size_t row = 0; row < kContextMenuRows; ++row)
    {
      const auto& item = itemAt(row, column);
      if (item.value.empty())
      {
        continue;
      }
      ui::Rect entry = KeyValueEntryRect(content, canvas.Height(), columns, row, column);
      if (columns > 1 && column == 0 && itemAt(row, 1).value.empty())
      {
        entry.width = content.width;
      }
      const bool focused = item.pressed || item.active;
      const ui::Color valueColor = item.playing ? kPlayback : (focused ? kAccent : kText);
      // Engines often describe "ALG 5"; the key label already says ALG.
      std::string value = item.value;
      if (value.size() > item.label.size() + 1 && value.compare(0, item.label.size(), item.label) == 0 && value[item.label.size()] == ' ')
      {
        value.erase(0, item.label.size() + 1);
      }
      canvas.FillRect(InsetRect(entry, 0, 1), FadeColor(focused ? kSelection : kSurface, alpha));
      const bool stacked = entry.width < 100;
      const int valueX = stacked ? entry.x + 3 : entry.x + labelWidth + 9;
      const int valueScale = stacked ? 1 : FitScale(canvas, value, entry.x + entry.width - valueX - 3, 2);
      const int labelTop = stacked ? entry.y + 1 : entry.y + (entry.height - InkHeight(1)) / 2;
      const int valueTop = stacked ? entry.y + 12 : entry.y + (entry.height - InkHeight(valueScale)) / 2;
      DrawInk(canvas, entry.x + 3, labelTop, TruncateText(canvas, item.label, entry.width - 6, 1),
              FadeColor(focused ? kAccent : kDim, alpha), 1);
      DrawInkClipped(canvas, entry, valueX, valueTop,
                     TruncateText(canvas, value, entry.x + entry.width - valueX - 3, valueScale),
                     FadeColor(valueColor, alpha), valueScale);
      if (item.level >= 0.0f)
      {
        DrawMeter(canvas, MakeRect(entry.x, entry.y + entry.height - 1, entry.width, 2), item.level,
                  focused ? kAccent : kDim, alpha);
      }
    }
  }
}

void DrawListRows(ui::Canvas& canvas, const ui::Rect& rect, const std::string& encodedLines, float alpha)
{
  int y = rect.y;
  std::size_t start = 0;
  while (start <= encodedLines.size() && y + kListRowPitch <= rect.y + rect.height)
  {
    const std::size_t newline = encodedLines.find('\n', start);
    const std::string line = encodedLines.substr(start, newline == std::string::npos ? std::string::npos : newline - start);
    const char marker = line.empty() ? '\0' : line.front();
    const bool encoded = marker == '>' || marker == ' ' || marker == '*';
    const std::string text = TruncateText(canvas, encoded ? line.substr(1) : line, rect.width, 1);
    const int inkTop = y + (kListRowPitch - InkHeight(1)) / 2;
    if (marker == '>')
    {
      canvas.FillRect(MakeRect(rect.x - 3, y, rect.width + 6, kListRowPitch), FadeColor(kAccent, alpha));
      DrawInk(canvas, rect.x, inkTop, text, FadeColor(kInverseText, alpha), 1);
    }
    else
    {
      // '*' marks the item already loaded on this track; plain lines are empty-state notes.
      const ui::Color color = marker == '*' ? kAccent : (encoded ? kText : kDim);
      DrawInk(canvas, rect.x, inkTop, text, FadeColor(color, alpha), 1);
    }
    if (newline == std::string::npos)
    {
      break;
    }
    start = newline + 1;
    y += kListRowPitch;
  }
}

void DrawTextEntry(ui::Canvas& canvas, const ui::Rect& rect, std::string_view text, float alpha)
{
  const int caretGap = 2;
  const int scale = InkWidth(canvas, text, 2) + caretGap + 2 <= rect.width ? 2 : 1;
  const int inkTop = rect.y + 26;
  const std::string visible = TruncateText(canvas, std::string(text), rect.width - caretGap - 2, scale);
  DrawInk(canvas, rect.x, inkTop, visible, FadeColor(kText, alpha), scale);
  const int caretX = rect.x + InkWidth(canvas, visible, scale) + (visible.empty() ? 0 : caretGap);
  canvas.FillRect(MakeRect(caretX, inkTop - 1, 2, InkHeight(scale) + 2), FadeColor(kAccent, alpha));
  canvas.FillRect(MakeRect(rect.x, inkTop + InkHeight(2) + 7, rect.width, 1), FadeColor(kRule, alpha));
}

std::string WrapTextToWidth(const ui::Canvas& canvas, std::string_view text, int maxWidth, int scale, int maxLines)
{
  if (text.empty() || maxWidth <= 0 || maxLines <= 0)
  {
    return {};
  }

  std::vector<std::string> lines;
  std::string current;
  std::size_t index = 0;
  while (index <= text.size() && static_cast<int>(lines.size()) < maxLines)
  {
    const std::size_t end = text.find_first_of(" \n", index);
    const std::string word(text.substr(index, end == std::string_view::npos ? std::string_view::npos : end - index));
    const std::string candidate = current.empty() ? word : current + " " + word;
    if (!word.empty() && InkWidth(canvas, candidate, scale) > maxWidth && !current.empty())
    {
      lines.push_back(current);
      current = word;
    }
    else if (!word.empty())
    {
      current = candidate;
    }
    if (end == std::string_view::npos)
    {
      break;
    }
    if (text[end] == '\n' && !current.empty())
    {
      lines.push_back(current);
      current.clear();
    }
    index = end + 1;
  }
  if (!current.empty() && static_cast<int>(lines.size()) < maxLines)
  {
    lines.push_back(current);
  }

  std::string wrapped;
  for (std::size_t line = 0; line < lines.size(); ++line)
  {
    wrapped += (line > 0 ? "\n" : "") + TruncateText(canvas, lines[line], maxWidth, scale);
  }
  return wrapped;
}

void DrawSubject(ui::Canvas& canvas, const ui::Rect& rect, std::string_view subject, std::string_view note, bool dangerous, float alpha)
{
  int y = rect.y + 14;
  if (!subject.empty())
  {
    const int scale = FitScale(canvas, subject, rect.width, 2);
    DrawMarqueeTextInRect(canvas, MakeRect(rect.x, y, rect.width, InkHeight(scale)), subject, FadeColor(kText, alpha), scale);
    y += InkHeight(scale) + 11;
  }
  const std::string wrapped = WrapTextToWidth(canvas, note, rect.width, 1, 3);
  std::size_t start = 0;
  while (!wrapped.empty() && start <= wrapped.size())
  {
    const std::size_t newline = wrapped.find('\n', start);
    DrawInk(canvas, rect.x, y, wrapped.substr(start, newline == std::string::npos ? std::string::npos : newline - start),
            FadeColor(dangerous ? kDanger : kDim, alpha), 1);
    if (newline == std::string::npos) break;
    start = newline + 1;
    y += kListRowPitch;
  }
}

void DrawAdjustValue(ui::Canvas& canvas,
                     const ui::Rect& rect,
                     std::string_view name,
                     std::string_view value,
                     float level,
                     float alpha)
{
  int y = rect.y + 10;
  if (!name.empty())
  {
    DrawInk(canvas, rect.x, y, TruncateText(canvas, std::string(name), rect.width, 1), FadeColor(kDim, alpha), 1);
    y += InkHeight(1) + 6;
  }

  // "120 BPM" keeps the number large and sets the unit beside it in small dim type.
  const std::size_t space = value.find(' ');
  const std::string main(space == std::string_view::npos ? value : value.substr(0, space));
  const std::string unit(space == std::string_view::npos ? std::string_view{} : value.substr(space + 1));
  const int unitWidth = unit.empty() ? 0 : InkWidth(canvas, unit, 1) + 5;
  const int scale = FitScale(canvas, main, rect.width - unitWidth, 4);
  const std::string mainText = TruncateText(canvas, main, rect.width - unitWidth, scale);
  DrawInk(canvas, rect.x, y, mainText, FadeColor(kText, alpha), scale);
  if (!unit.empty())
  {
    DrawInk(canvas, rect.x + InkWidth(canvas, mainText, scale) + 5, y + InkHeight(scale) - InkHeight(1), unit, FadeColor(kDim, alpha), 1);
  }
  y += InkHeight(scale) + 11;
  DrawMeter(canvas, MakeRect(rect.x, y, rect.width, 3), level, kAccent, alpha);
}

void DrawMiniPiano(ui::Canvas& canvas,
                   const ui::Rect& rect,
                   const UiScreenSnapshot& snapshot,
                   const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                   float alpha)
{
  // Light pitch classes from the notes actually mapped to the held keys, whatever the layout or octave.
  std::array<bool, 12> held{};
  for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
  {
    for (std::size_t column = 0; column < kHomePerformanceColumnCount; ++column)
    {
      const int note = snapshot.performanceKeyNotes[row * kHomePerformanceColumnCount + column];
      if (note >= 0 && activeKeys[row * kKeyboardMatrixColumns + kHomePerformanceFirstColumn + column])
      {
        held[static_cast<std::size_t>(note % 12)] = true;
      }
    }
  }

  constexpr std::array<std::size_t, 7> kWhite = {0, 2, 4, 5, 7, 9, 11};
  constexpr std::array<std::size_t, 5> kBlack = {1, 3, 6, 8, 10};
  constexpr std::array<int, 5> kBlackAfterWhite = {0, 1, 3, 4, 5};
  constexpr int kWhiteCount = 7;
  constexpr int kKeyGap = 1;
  constexpr int kBlackMoat = 1;
  canvas.FillRect(rect, FadeColor(kSurface, alpha));
  canvas.StrokeRect(rect, FadeColor(kRule, alpha));
  const int padding = std::max(3, rect.height / 8);
  const ui::Rect bed = InsetRect(rect, padding, padding);
  canvas.FillRect(bed, FadeColor(kRule, alpha));
  // Equal white keys on one pitch; leftover pixels become symmetric margins.
  const int pitch = (bed.width + kKeyGap) / kWhiteCount;
  const int whiteWidth = pitch - kKeyGap;
  const int left = bed.x + (bed.width - (kWhiteCount * pitch - kKeyGap)) / 2;
  for (int i = 0; i < kWhiteCount; ++i)
  {
    canvas.FillRect(MakeRect(left + i * pitch, bed.y, whiteWidth, bed.height),
                    FadeColor(held[kWhite[static_cast<std::size_t>(i)]] ? kAccent : kPianoWhite, alpha));
  }
  // A black key straddles one gap; matching its width parity to the gap centres it exactly.
  int blackWidth = std::max(3, whiteWidth * 3 / 5);
  if ((blackWidth - kKeyGap) % 2 != 0) --blackWidth;
  const int blackHeight = std::max(4, bed.height * 3 / 5);
  for (std::size_t i = 0; i < kBlack.size(); ++i)
  {
    const int gapX = left + (kBlackAfterWhite[i] + 1) * pitch - kKeyGap;
    const ui::Rect key = MakeRect(gapX + (kKeyGap - blackWidth) / 2, bed.y, blackWidth, blackHeight);
    // A neutral moat separates lit adjacent keys without reversing the piano.
    canvas.FillRect(MakeRect(key.x - kBlackMoat, key.y, key.width + 2 * kBlackMoat, key.height + kBlackMoat), FadeColor(kRule, alpha));
    canvas.FillRect(key, FadeColor(held[kBlack[i]] ? kAccent : kPianoBlack, alpha));
  }
}

void DrawPerformanceKeyMap(ui::Canvas& canvas,
                           int x,
                           int y,
                           const UiScreenSnapshot& snapshot,
                           const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys,
                           float alpha)
{
  constexpr int kKeyHeight = 10;
  constexpr int kRowGap = 3;
  const bool piano = snapshot.keyboardLayoutMode == KeyboardLayoutMode::Piano;
  const ui::Color plain = FadeColor(kSurface, alpha);
  const ui::Color root = FadeColor(kSelection, alpha);
  const ui::Color dark = FadeColor(kPianoBlack, alpha);
  const ui::Color edge = FadeColor(kRule, alpha);

  for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
  {
    for (std::size_t column = 0; column < kHomePerformanceColumnCount; ++column)
    {
      const std::size_t index = row * kHomePerformanceColumnCount + column;
      const int note = snapshot.performanceKeyNotes[index];
      if (note < 0)
      {
        continue;
      }
      const ui::Rect key = MakeRect(x + static_cast<int>(column) * (kKeyColumnWidth + kKeyColumnGap),
                                    y + static_cast<int>(row) * (kKeyHeight + kRowGap),
                                    kKeyColumnWidth,
                                    kKeyHeight);
      const bool pressed = activeKeys[row * kKeyboardMatrixColumns + kHomePerformanceFirstColumn + column];
      const bool isRoot = note % 12 == snapshot.keyboardRootNote;
      if (pressed)
      {
        canvas.FillRect(key, FadeColor(kAccent, alpha));
      }
      else if (piano)
      {
        // Even rows from the bottom carry white keys, odd rows the sharps.
        const bool whiteRow = ((kKeyboardMatrixRows - 1 - row) % 2) == 0;
        canvas.FillRect(key, isRoot ? root : (whiteRow ? plain : dark));
        canvas.StrokeRect(key, isRoot ? FadeColor(kAccent, alpha) : edge);
      }
      else if (snapshot.performanceKeyInScale[index])
      {
        canvas.FillRect(key, isRoot ? root : plain);
        canvas.StrokeRect(key, isRoot ? FadeColor(kAccent, alpha) : edge);
      }
      else
      {
        canvas.StrokeRect(key, edge);
      }
    }
  }
}

std::string OctaveStatusText(const UiScreenSnapshot& snapshot)
{
  if (snapshot.keyboardOctave == 0 && !snapshot.keyboardOctaveChanged) return {};
  return std::string("OCT ") + (snapshot.keyboardOctave > 0 ? "+" : "") + std::to_string(snapshot.keyboardOctave);
}

std::string HeldNoteNames(const UiScreenSnapshot& snapshot,
                          const std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>& activeKeys)
{
  std::vector<int> notes;
  for (std::size_t row = 0; row < kKeyboardMatrixRows; ++row)
  {
    for (std::size_t column = 0; column < kHomePerformanceColumnCount; ++column)
    {
      const int note = snapshot.performanceKeyNotes[row * kHomePerformanceColumnCount + column];
      if (note >= 0 && activeKeys[row * kKeyboardMatrixColumns + kHomePerformanceFirstColumn + column])
      {
        notes.push_back(note);
      }
    }
  }
  std::sort(notes.begin(), notes.end());
  notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
  std::string names;
  for (int note : notes)
  {
    names += (names.empty() ? "" : " ") + NoteName(note);
  }
  return names;
}

void DrawLoopBar(ui::Canvas& canvas,
                 const ui::Rect& rect,
                 std::size_t segmentCount,
                 std::size_t activeSegment,
                 float progress,
                 bool playing,
                 float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  segmentCount = std::clamp<std::size_t>(segmentCount, 1, kSequencerMaxPatternCount);
  activeSegment = std::min(activeSegment, segmentCount - 1);
  const int count = static_cast<int>(segmentCount);
  for (int index = 0; index < count; ++index)
  {
    const int left = rect.x + (rect.width + kLoopBarGap) * index / count;
    const int right = rect.x + (rect.width + kLoopBarGap) * (index + 1) / count - kLoopBarGap;
    const ui::Rect segment = MakeRect(left, rect.y, std::max(1, right - left), rect.height);
    if (static_cast<std::size_t>(index) != activeSegment)
    {
      canvas.FillRect(segment, FadeColor(kRule, alpha));
      continue;
    }

    // The current Pattern is cyan; while playing, green fills it with the playhead.
    if (!playing)
    {
      canvas.FillRect(segment, FadeColor(kAccent, alpha));
      continue;
    }
    canvas.FillRect(segment, FadeColor(MixColor(kRule, kAccent, 0.4f), alpha));
    const int fill = std::clamp(static_cast<int>(static_cast<float>(segment.width) * Clamp01(progress) + 0.5f), 0, segment.width);
    if (fill > 0)
    {
      canvas.FillRect(MakeRect(segment.x, segment.y, fill, segment.height), FadeColor(kPlayback, alpha));
    }
  }
}

}  // namespace synth::ui_render
