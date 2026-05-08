#include "Applications/Synth/UI/SynthUiRenderer.h"

#include "Applications/Synth/UI/Dx7/Dx7UiRenderer.h"
#include "Applications/Synth/UI/Juno/JunoUiRenderer.h"
#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>

namespace synth {

namespace {

using namespace ui_render;

void DrawSoundContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  if (!snapshot.showSecondaryText)
  {
    canvas.DrawText(rect.x,
                    rect.y + 10,
                    TruncateText(canvas, snapshot.headerPrimary, rect.width, 1),
                    accent,
                    1);
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 34, rect.width, 34),
                          TruncateText(canvas, snapshot.primaryText, rect.width, 3),
                          text,
                          3,
                          ui::TextAlign::Left,
                          ui::VerticalAlign::Middle);
    return;
  }

  if (snapshot.showPresetBrowser)
  {
    canvas.DrawText(rect.x,
                    rect.y + 10,
                    TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                    accent,
                    2);
    canvas.DrawLine(rect.x,
                    rect.y + 34,
            rect.x + rect.width - 1,
                    rect.y + 34,
                    divider);

    const int lineScale = 1;
    const int lineHeight = canvas.FontHeight(lineScale) + 2;
    int lineY = rect.y + 42;
    std::size_t start = 0;
    for (int lineIndex = 0; lineIndex < 7 && start <= snapshot.secondaryText.size(); ++lineIndex)
    {
      const std::size_t newline = snapshot.secondaryText.find('\n', start);
      const std::string encodedLine = newline == std::string::npos
                                          ? snapshot.secondaryText.substr(start)
                                          : snapshot.secondaryText.substr(start, newline - start);
      const bool selected = !encodedLine.empty() && encodedLine.front() == '>';
      const std::string line = !encodedLine.empty() && (encodedLine.front() == '>' || encodedLine.front() == ' ')
                                   ? encodedLine.substr(1)
                                   : encodedLine;
      const ui::Color lineColor = selected ? accent : text;
                      canvas.DrawText(rect.x,
                      lineY,
                              TruncateText(canvas, line, rect.width, lineScale),
                      lineColor,
                      lineScale);
      if (newline == std::string::npos)
      {
        break;
      }

      start = newline + 1;
      lineY += lineHeight;
    }
    return;
  }

  canvas.DrawText(rect.x,
                  rect.y + 10,
                  TruncateText(canvas, snapshot.headerPrimary, rect.width, 1),
                  accent,
                  1);

  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 30, rect.width, 24),
                        TruncateText(canvas, snapshot.primaryText, rect.width, 2),
                        text,
                        2,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
  canvas.DrawLine(rect.x,
                  rect.y + 58,
                  rect.x + rect.width - 1,
                  rect.y + 58,
                  divider);
  const int presetScale = canvas.MeasureTextWidth(snapshot.secondaryText, 5) <= rect.width ? 5 : 4;
  DrawMarqueeTextInRect(canvas,
                        MakeRect(rect.x, rect.y + 64, rect.width, 42),
                        snapshot.secondaryText,
                        text,
                        presetScale,
                        ui::TextAlign::Left);
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
  const int headerX = rect.x + 4;
  const int headerY = rect.y + 8;
  const int headerGap = 8;
  const int trackLabelWidth = std::min(std::max(56, canvas.MeasureTextWidth(snapshot.headerSecondary, 1) + 6),
                                       std::max(56, rect.width / 2));
  const int trackLabelX = rect.x + rect.width - 4 - trackLabelWidth;
  const int titleWidth = std::max(0, trackLabelX - headerGap - headerX);

  canvas.DrawText(headerX,
                  headerY,
                  TruncateText(canvas, snapshot.headerPrimary, titleWidth, 1),
                  accent,
                  1);
  DrawHomeTrackLabel(canvas,
                     MakeRect(trackLabelX, headerY, trackLabelWidth, 10),
                     snapshot.headerSecondary,
                     alpha);
  canvas.DrawLine(rect.x + 4, rect.y + 24, rect.x + rect.width - 5, rect.y + 24, divider);
  DrawMarqueeTextInRect(canvas,
                        MakeRect(rect.x + 4, rect.y + 34, rect.width - 8, 34),
                        snapshot.primaryText,
                        text,
                        3,
                        ui::TextAlign::Center);

  DrawMiniKeyboard(canvas, MakeRect(rect.x + 16, rect.y + 74, rect.width - 32, 28), activeKeys, alpha);
  DrawThinLooperBar(canvas, MakeRect(rect.x + 4, rect.y + rect.height - 11, rect.width - 8, 4), snapshot.looperProgress, alpha);
}

void DrawContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  if (IsDx7ParameterSnapshot(snapshot))
  {
    DrawDx7ParameterContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (IsJunoParameterSnapshot(snapshot))
  {
    DrawJunoParameterContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (snapshot.modifier == UiModifier::Sound)
  {
    DrawSoundContextContent(canvas, rect, snapshot, alpha);
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);

  const int titleScale = snapshot.showSecondaryText ? 2 : 2;
  const int titleY = rect.y + 10;
  const int titleHeight = canvas.FontHeight(titleScale);
  const std::string title = snapshot.primaryText.empty() ? snapshot.headerPrimary : snapshot.primaryText;
  canvas.DrawText(rect.x,
                  titleY,
                  TruncateText(canvas, title, rect.width, titleScale),
                  accent,
                  titleScale);

  const int dividerY = titleY + titleHeight + 6;
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  if (!snapshot.showSecondaryText)
  {
    return;
  }

  canvas.DrawTextInRect(MakeRect(rect.x,
                                 dividerY + 8,
                                 rect.width,
                                 rect.height - (dividerY - rect.y) - 12),
                        TruncateText(canvas, snapshot.secondaryText, rect.width, 5),
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
  ui_render::AdvanceMarqueeFrame();
  canvas.Clear(ui_render::kBackground);

  if (model.Transition().active)
  {
    DrawTransitionFrame(canvas, model.Transition(), activeKeys);
    return;
  }

  const UiScreenSnapshot snapshot = model.CurrentScreenSnapshot(engine);
  DrawSnapshotFrame(canvas, snapshot, LayoutForSnapshot(canvas, snapshot), activeKeys, 1.0f);
}

}  // namespace synth