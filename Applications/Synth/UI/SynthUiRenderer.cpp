#include "Applications/Synth/UI/SynthUiRenderer.h"

#include "Applications/Synth/UI/Dx7/Dx7UiRenderer.h"
#include "Applications/Synth/UI/Juno/JunoUiRenderer.h"
#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace synth {

namespace {

using namespace ui_render;

constexpr float kRecordingWaveformNoiseFloor = 0.02f;
constexpr float kRecordingWaveformDisplayThreshold = 0.08f;

bool IsKeyboardLayoutSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Layout;
}

int FitTextScale(const ui::Canvas& canvas, std::string_view text, int maxWidth, int maxScale, int minScale)
{
  int scale = maxScale;
  while (scale > minScale && canvas.MeasureTextWidth(text, scale) > maxWidth)
  {
    --scale;
  }
  return scale;
}

const char* KeyboardOverviewModeText(KeyboardLayoutMode mode)
{
  switch (mode)
  {
    case KeyboardLayoutMode::Piano:
      return "Piano Mode";
    case KeyboardLayoutMode::Chromatic:
      return "Chromatic Mode";
    case KeyboardLayoutMode::Octave:
    default:
      return "Scale Mode";
  }
}

ui::Color KeyboardFieldColor(const UiScreenSnapshot& snapshot,
                             std::string_view itemId,
                             ui::Color activeColor,
                             ui::Color defaultColor,
                             ui::Color disabledColor)
{
  if ((itemId == "lay.root" && snapshot.keyboardRootDisabled) ||
      (itemId == "lay.scl" && snapshot.keyboardScaleDisabled))
  {
    return disabledColor;
  }

  return snapshot.focusItemId == itemId ? activeColor : defaultColor;
}

void DrawKeyboardContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const int titleScale = 2;
  const int titleY = rect.y + 10;
  const int dividerY = titleY + canvas.FontHeight(titleScale) + 6;

  canvas.DrawText(rect.x,
                  titleY,
                  TruncateText(canvas, snapshot.headerPrimary.empty() ? std::string{"Keyboard"} : snapshot.headerPrimary, rect.width, titleScale),
                  accent,
                  titleScale);
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  const ui::Rect body = MakeRect(rect.x,
                                 dividerY + 6,
                                 rect.width,
                                 std::max(0, rect.height - (dividerY - rect.y) - 10));

  if (snapshot.keyboardLayoutMode == KeyboardLayoutMode::Piano)
  {
    const ui::Color modeColor = text;
    const int lineGap = 1;
    const int pianoScale = FitTextScale(canvas, "Piano", body.width, 5, 3);
    const int modeScale = FitTextScale(canvas, "Mode", body.width, 5, 3);
    const int pianoHeight = canvas.FontHeight(pianoScale) + 2;
    const int modeHeight = canvas.FontHeight(modeScale) + 2;
    const ui::Rect pianoRect = MakeRect(body.x, body.y + 2, body.width, pianoHeight);
    const ui::Rect modeRect = MakeRect(body.x,
                                       pianoRect.y + pianoRect.height + lineGap,
                                       body.width,
                                       modeHeight);

    DrawMarqueeTextInRect(canvas, pianoRect, "Piano", text, pianoScale, ui::TextAlign::Left);
    DrawMarqueeTextInRect(canvas, modeRect, "Mode", modeColor, modeScale, ui::TextAlign::Left);
    return;
  }

  const int modeScale = 2;
  const int rootScale = FitTextScale(canvas, snapshot.keyboardRootName, body.width, 5, 3);
  const int scaleScale = FitTextScale(canvas, snapshot.keyboardScaleName, body.width, 4, 2);
  const int rootHeight = canvas.FontHeight(rootScale) + 2;
  const int scaleHeight = canvas.FontHeight(scaleScale) + 2;
  const int modeHeight = canvas.FontHeight(modeScale) + 2;
  const int sectionGap = 2;

  const ui::Rect rootValueRect = MakeRect(body.x,
                                          body.y + 2,
                                          body.width,
                                          rootHeight);
  const ui::Rect scaleValueRect = MakeRect(body.x,
                                           rootValueRect.y + rootValueRect.height + sectionGap,
                                           body.width,
                                           scaleHeight);
  const ui::Rect modeRect = MakeRect(body.x,
                                     scaleValueRect.y + scaleValueRect.height + sectionGap,
                                     body.width,
                                     modeHeight);

  const ui::Color rootColor = KeyboardFieldColor(snapshot, "lay.root", accent, text, divider);
  const ui::Color scaleColor = KeyboardFieldColor(snapshot, "lay.scl", accent, text, divider);
  const ui::Color modeColor = text;

  DrawMarqueeTextInRect(canvas, rootValueRect, snapshot.keyboardRootName, rootColor, rootScale, ui::TextAlign::Left);
  DrawMarqueeTextInRect(canvas, scaleValueRect, snapshot.keyboardScaleName, scaleColor, scaleScale, ui::TextAlign::Left);
  canvas.DrawTextInRect(modeRect,
                        KeyboardOverviewModeText(snapshot.keyboardLayoutMode),
                        modeColor,
                        modeScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);
}

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
      const bool active = !encodedLine.empty() && encodedLine.front() == '*';
      const std::string line = !encodedLine.empty() && (encodedLine.front() == '>' || encodedLine.front() == ' ' || encodedLine.front() == '*')
                                   ? encodedLine.substr(1)
                                   : encodedLine;
      const ui::Color lineColor = selected || active ? accent : text;
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
  int presetScale = 5;
  while (presetScale > 1 && canvas.MeasureTextWidth(snapshot.secondaryText, presetScale) > rect.width)
  {
    --presetScale;
  }
  const ui::Rect valueRect = MakeRect(rect.x, rect.y + 64, rect.width, 42);
  if (canvas.MeasureTextWidth(snapshot.secondaryText, presetScale) <= valueRect.width)
  {
    canvas.DrawTextInRect(valueRect, snapshot.secondaryText, text, presetScale, ui::TextAlign::Left, ui::VerticalAlign::Middle);
    return;
  }

  DrawMarqueeTextInRect(canvas, valueRect, snapshot.secondaryText, text, presetScale, ui::TextAlign::Left);
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

std::string FormatSamplerSeconds(float seconds, bool fractional)
{
  const float clampedSeconds = std::max(0.0f, seconds);
  int wholeSeconds = static_cast<int>(std::floor(clampedSeconds));
  if (!fractional)
  {
    return std::to_string(wholeSeconds) + "s";
  }

  int tenths = static_cast<int>(std::floor((clampedSeconds - static_cast<float>(wholeSeconds)) * 10.0f + 0.5f));
  if (tenths >= 10)
  {
    ++wholeSeconds;
    tenths = 0;
  }

  return std::to_string(wholeSeconds) + "." + std::to_string(tenths) + "s";
}

std::string FormatSignedValue(int value)
{
  return value > 0 ? "+" + std::to_string(value) : std::to_string(value);
}

std::string FormatPercentValue(int value)
{
  return std::to_string((std::clamp(value, 0, 127) * 100) / 127) + "%";
}

std::string FormatSamplerRootNote(int value)
{
  static constexpr std::array<const char*, 12> kNoteNames = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
  const int note = std::clamp(value, 0, 127);
  return std::string(kNoteNames[static_cast<std::size_t>(note % 12)]) + std::to_string(note / 12 - 1);
}

std::string WrapTextToWidth(const ui::Canvas& canvas,
                            std::string_view text,
                            int maxWidth,
                            int scale,
                            int maxLines)
{
  if (text.empty() || maxWidth <= 0 || maxLines <= 0)
  {
    return {};
  }

  std::vector<std::string> lines;
  std::size_t paragraphStart = 0;
  while (paragraphStart <= text.size() && static_cast<int>(lines.size()) < maxLines)
  {
    const std::size_t paragraphEnd = text.find('\n', paragraphStart);
    std::string paragraph = std::string(paragraphEnd == std::string_view::npos
        ? text.substr(paragraphStart)
        : text.substr(paragraphStart, paragraphEnd - paragraphStart));

    if (paragraph.empty())
    {
      lines.push_back({});
    }
    else
    {
      std::size_t wordStart = 0;
      std::string currentLine;
      while (wordStart < paragraph.size() && static_cast<int>(lines.size()) < maxLines)
      {
        while (wordStart < paragraph.size() && paragraph[wordStart] == ' ')
        {
          ++wordStart;
        }
        if (wordStart >= paragraph.size())
        {
          break;
        }

        std::size_t wordEnd = paragraph.find(' ', wordStart);
        std::string word = paragraph.substr(wordStart, wordEnd == std::string::npos ? std::string::npos : wordEnd - wordStart);
        const std::string candidate = currentLine.empty() ? word : currentLine + " " + word;
        if (canvas.MeasureTextWidth(candidate, scale) <= maxWidth)
        {
          currentLine = candidate;
        }
        else
        {
          if (!currentLine.empty())
          {
            lines.push_back(currentLine);
            currentLine.clear();
            if (static_cast<int>(lines.size()) >= maxLines)
            {
              break;
            }
            continue;
          }

          currentLine = TruncateText(canvas, word, maxWidth, scale);
          lines.push_back(currentLine);
          currentLine.clear();
        }

        if (wordEnd == std::string::npos)
        {
          break;
        }
        wordStart = wordEnd + 1;
      }

      if (!currentLine.empty() && static_cast<int>(lines.size()) < maxLines)
      {
        lines.push_back(currentLine);
      }
    }

    if (paragraphEnd == std::string_view::npos)
    {
      break;
    }
    paragraphStart = paragraphEnd + 1;
  }

  if (lines.empty())
  {
    return {};
  }

  std::string wrapped;
  for (std::size_t index = 0; index < lines.size(); ++index)
  {
    if (index > 0)
    {
      wrapped += '\n';
    }
    wrapped += lines[index];
  }

  return wrapped;
}

void DrawSamplerOverviewStrip(ui::Canvas& canvas,
                              const ui::Rect& rect,
                              const std::string& label,
                              const std::string& value,
                              ui::Color accent,
                              ui::Color divider,
                              ui::Color text,
                              bool drawDivider)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const int labelWidth = std::max(20, canvas.MeasureTextWidth(label, 1));
  const int valueX = rect.x + labelWidth + 12;
  canvas.DrawText(rect.x, rect.y, TruncateText(canvas, label, labelWidth + 4, 1), accent, 1);
  canvas.DrawTextInRect(MakeRect(valueX,
                                 rect.y - 1,
                                 std::max(0, rect.x + rect.width - valueX),
                                 rect.height + 2),
                        TruncateText(canvas, value, std::max(0, rect.x + rect.width - valueX), 1),
                        text,
                        1,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);

  if (drawDivider)
  {
    canvas.DrawLine(rect.x, rect.y + rect.height, rect.x + rect.width - 1, rect.y + rect.height, divider);
  }
}

bool IsSamplerOverviewContext(const UiScreenSnapshot& snapshot)
{
  return snapshot.engineKind == SynthEngineKind::Sampler &&
         snapshot.modifier == UiModifier::Parameter &&
         !snapshot.samplerRecording &&
         !snapshot.samplerRecordConfirmMode &&
         snapshot.focusItemId.empty();
}

void DrawSamplerOverviewContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const std::string sampleTitle = snapshot.samplerHasDraft
      ? (snapshot.samplerDraftName.empty() ? std::string{"Unsaved draft"} : snapshot.samplerDraftName)
      : (snapshot.samplerSampleName.empty() ? std::string{"EMPTY"} : snapshot.samplerSampleName);
  const bool hasAudio = snapshot.samplerHasSample || snapshot.samplerHasDraft;
  const std::string status = hasAudio
      ? (snapshot.samplerStatusText.empty() ? std::string{"READY"} : snapshot.samplerStatusText)
      : std::string{};
  const int headerHeight = 24;
  const int statusWidth = std::min(84, std::max(48, rect.width / 3));

  canvas.DrawText(rect.x, rect.y + 8, "Sampler", accent, 1);
  if (!status.empty())
  {
    canvas.DrawTextInRect(MakeRect(rect.x + rect.width - statusWidth,
                                   rect.y + 8,
                                   statusWidth,
                                   12),
                          status,
                          accent,
                          1,
                          ui::TextAlign::Right,
                          ui::VerticalAlign::Middle);
  }
  canvas.DrawLine(rect.x, rect.y + headerHeight, rect.x + rect.width - 1, rect.y + headerHeight, divider);

  if (!hasAudio)
  {
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 42, rect.width, 34),
                          "Load from file\nor Record new",
                          accent,
                          1,
                          ui::TextAlign::Center,
                          ui::VerticalAlign::Middle);
    return;
  }

  const int titleScale = canvas.MeasureTextWidth(sampleTitle, 2) <= rect.width ? 2 : 1;
  canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 34, rect.width, 16),
                        TruncateText(canvas, sampleTitle, rect.width, titleScale),
                        text,
                        titleScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Middle);

  const int rowGap = 1;
  const int rowHeight = canvas.FontHeight(1) + 2;
  const int stripsTop = rect.y + 56;
  const float sampleSeconds = snapshot.samplerSampleRate == 0
      ? 0.0f
      : static_cast<float>(snapshot.samplerFrameCount) / static_cast<float>(snapshot.samplerSampleRate);
  const std::string playMode = snapshot.samplerLoop != 0 ? "LOOP" : "1SHOT";

  DrawSamplerOverviewStrip(canvas,
                           MakeRect(rect.x, stripsTop, rect.width, rowHeight),
                           "PLAY",
                 FormatSamplerRootNote(snapshot.samplerRoot) + "  " + playMode,
                           accent,
                           divider,
                           text,
                           true);
  DrawSamplerOverviewStrip(canvas,
                           MakeRect(rect.x, stripsTop + (rowHeight + rowGap), rect.width, rowHeight),
                           "LEN",
                           FormatSamplerSeconds(sampleSeconds, true),
                           accent,
                           divider,
                           text,
                           false);
}

bool IsSamplerTrimWaveformFocus(const std::string& focusItemId)
{
  return focusItemId == "par.sampler.group.trim" || focusItemId == "sample.start" || focusItemId == "sample.end";
}

std::string SamplerWaveformTitle(const UiScreenSnapshot& snapshot)
{
  if (snapshot.focusItemId == "par.sampler.page.save")
  {
    return "Save";
  }
  return "Trim";
}

void DrawSamplerWaveform(ui::Canvas& canvas,
                         const ui::Rect& rect,
                         const UiScreenSnapshot& snapshot,
                         float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color cursor = FadeColor(kText, alpha);
  canvas.StrokeRect(rect, divider);

  const bool recording = snapshot.samplerRecording;
  const int usableWaveformWidth = std::max(1, rect.width - 4);
  const int waveformWidth = usableWaveformWidth;
  const int midY = rect.y + rect.height / 2;
  if (!recording)
  {
    canvas.DrawLine(rect.x + 1, midY, rect.x + rect.width - 2, midY, divider);
  }

  const bool zoomTrim = false &&
                        snapshot.samplerFrameCount > 0 && !snapshot.samplerRecording;
  const std::uint32_t totalFrames = std::max<std::uint32_t>(1, snapshot.samplerFrameCount);
  std::uint32_t displayStartFrame = 0;
  std::uint32_t displayEndFrame = totalFrames;
  if (zoomTrim)
  {
    displayStartFrame = std::min(snapshot.samplerTrimStartFrame, totalFrames - 1);
    displayEndFrame = std::clamp<std::uint32_t>(std::max(snapshot.samplerTrimEndFrame, displayStartFrame + 1),
                                                displayStartFrame + 1,
                                                totalFrames);
  }
  const std::uint32_t displayFrameSpan = std::max<std::uint32_t>(1, displayEndFrame - displayStartFrame);

  int startX = rect.x + 1;
  int endX = rect.x + rect.width - 2;
  if (!zoomTrim && snapshot.samplerFrameCount > 0 && !snapshot.samplerRecording)
  {
    const int usableWidth = std::max(1, rect.width - 3);
    startX = rect.x + 1 + static_cast<int>((static_cast<std::uint64_t>(snapshot.samplerTrimStartFrame) * usableWidth) /
                                           std::max<std::uint32_t>(1, snapshot.samplerFrameCount));
    endX = rect.x + 1 + static_cast<int>((static_cast<std::uint64_t>(snapshot.samplerTrimEndFrame) * usableWidth) /
                                         std::max<std::uint32_t>(1, snapshot.samplerFrameCount));
  }

  if (!zoomTrim && snapshot.samplerFrameCount > 0 && !snapshot.samplerRecording)
  {
    const int selectionLeft = std::min(startX, endX);
    const int selectionRight = std::max(startX, endX);
    const int selectionWidth = std::max(0, selectionRight - selectionLeft - 1);
    if (selectionWidth > 0)
    {
      canvas.FillRect(MakeRect(selectionLeft + 1,
                               rect.y + 1,
                               selectionWidth,
                               std::max(0, rect.height - 2)),
                      MixColor(kBackground, accent, 0.28f));
    }
  }

  bool hasVisibleWaveform = false;

  for (std::size_t bin = 0; bin < snapshot.samplerWaveform.size(); ++bin)
  {
    const int x0 = rect.x + 2 + static_cast<int>((bin * waveformWidth) / snapshot.samplerWaveform.size());
    int x1 = rect.x + 2 + static_cast<int>(((bin + 1) * waveformWidth) / snapshot.samplerWaveform.size()) - 1;
    if (bin + 1 == snapshot.samplerWaveform.size())
    {
      x1 = rect.x + 1 + waveformWidth;
    }
    const int xEnd = std::max(x0, x1);

    float peak = snapshot.samplerWaveform[bin];
    if (zoomTrim)
    {
      const std::uint32_t srcFrameStart = displayStartFrame + static_cast<std::uint32_t>((static_cast<std::uint64_t>(displayFrameSpan) * bin) /
                                                                                          snapshot.samplerWaveform.size());
      std::uint32_t srcFrameEnd = displayStartFrame + static_cast<std::uint32_t>((static_cast<std::uint64_t>(displayFrameSpan) * (bin + 1)) /
                                                                                 snapshot.samplerWaveform.size());
      srcFrameEnd = std::max(srcFrameEnd, srcFrameStart + 1);
      const std::size_t srcBinBegin = std::min(snapshot.samplerWaveform.size() - 1,
                                               static_cast<std::size_t>((static_cast<std::uint64_t>(srcFrameStart) * snapshot.samplerWaveform.size()) /
                                                                        totalFrames));
      const std::size_t srcBinEnd = std::max(srcBinBegin + 1,
                                             std::min(snapshot.samplerWaveform.size(),
                                                      static_cast<std::size_t>((static_cast<std::uint64_t>(srcFrameEnd) * snapshot.samplerWaveform.size() + totalFrames - 1) /
                                                                               totalFrames)));
      peak = 0.0f;
      for (std::size_t sourceBin = srcBinBegin; sourceBin < srcBinEnd; ++sourceBin)
      {
        peak = std::max(peak, snapshot.samplerWaveform[sourceBin]);
      }
    }

    if (recording)
    {
      peak = std::clamp((std::clamp(peak, 0.0f, 1.0f) - kRecordingWaveformNoiseFloor) /
                            (1.0f - kRecordingWaveformNoiseFloor),
                        0.0f,
                        1.0f);
      if (peak < kRecordingWaveformDisplayThreshold)
      {
        peak = 0.0f;
      }
      peak = std::pow(peak, 0.55f);
    }

    if (peak <= 0.0f)
    {
      continue;
    }

    hasVisibleWaveform = true;

    int halfHeight = std::max(1, static_cast<int>(peak * static_cast<float>(rect.height / 2 - 3)));
    if (recording && peak > 0.0f)
    {
      halfHeight = std::max(2, halfHeight);
    }
    for (int x = x0; x <= xEnd; ++x)
    {
      canvas.DrawLine(x, midY - halfHeight, x, midY + halfHeight, accent);
    }
  }

  const int usableWidth = std::max(1, rect.width - 3);
  if (recording)
  {
    if (!hasVisibleWaveform)
    {
      canvas.DrawLine(rect.x + 3,
                      midY,
                      rect.x + rect.width - 4,
                      midY,
                      MixColor(divider, accent, 0.2f));
    }
    return;
  }

  if (snapshot.samplerFrameCount == 0)
  {
    return;
  }

  if (!zoomTrim)
  {
    canvas.DrawLine(startX, rect.y + 1, startX, rect.y + rect.height - 2, accent);
    canvas.DrawLine(endX, rect.y + 1, endX, rect.y + rect.height - 2, accent);
  }

  const std::uint32_t cursorFrame = snapshot.samplerPreviewing ? snapshot.samplerPreviewCursorFrame
                                                               : static_cast<std::uint32_t>((snapshot.samplerTrimCursor * snapshot.samplerFrameCount) / 127);
  const std::uint32_t clampedCursorFrame = displayEndFrame > displayStartFrame
      ? std::clamp(cursorFrame, displayStartFrame, displayEndFrame - 1)
      : displayStartFrame;
  const int cursorX = rect.x + 1 + static_cast<int>((static_cast<std::uint64_t>(clampedCursorFrame - displayStartFrame) * usableWidth) /
                                                    displayFrameSpan);
  canvas.DrawLine(cursorX, rect.y + 3, cursorX, rect.y + rect.height - 4, cursor);
}

void DrawSamplerProgress(ui::Canvas& canvas, const ui::Rect& rect, float progress, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color divider = FadeColor(kDivider, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  canvas.StrokeRect(rect, divider);
  const int fillWidth = std::clamp(static_cast<int>(static_cast<float>(rect.width - 2) * Clamp01(progress)), 0, rect.width - 2);
  if (fillWidth > 0)
  {
    canvas.FillRect(MakeRect(rect.x + 1, rect.y + 1, fillWidth, std::max(0, rect.height - 2)), accent);
  }
}

void DrawSamplerRecordPanel(ui::Canvas& canvas,
                            const ui::Rect& rect,
                            const UiScreenSnapshot& snapshot,
                            float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const bool recording = snapshot.samplerRecording;
  const bool inputError = snapshot.samplerRecordConfirmMode && snapshot.samplerStatusText == "INPUT ERR";
  const std::string headline = inputError ? "INPUT ERR" : (recording ? "RECORDING" : "START REC");
  const std::string detail = inputError ? "Check mic input" : "Capture to draft";

  canvas.StrokeRect(rect, divider);

  const int panelPadding = 8;
  const int progressReservedHeight = recording ? 20 : 0;
  const int contentHeight = std::max(0, rect.height - panelPadding * 2 - progressReservedHeight);
  const int headlineHeight = canvas.FontHeight(2);
  const int detailHeight = canvas.FontHeight(1);
  const int textGap = 8;
  const int textBlockHeight = headlineHeight + textGap + detailHeight;
  const int textTop = rect.y + panelPadding + std::max(0, (contentHeight - textBlockHeight) / 2);
  const ui::Rect headlineRect = MakeRect(rect.x + panelPadding,
                                         textTop,
                                         std::max(0, rect.width - panelPadding * 2),
                                         headlineHeight + 2);
  const ui::Rect detailRect = MakeRect(rect.x + panelPadding,
                                       textTop + headlineHeight + textGap - 1,
                                       std::max(0, rect.width - panelPadding * 2),
                                       detailHeight + 2);

  canvas.DrawTextInRect(headlineRect,
                        headline,
                        accent,
                        2,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);
  canvas.DrawTextInRect(detailRect,
                        detail,
                        text,
                        1,
                        ui::TextAlign::Center,
                        ui::VerticalAlign::Middle);

  if (!recording)
  {
    return;
  }

  const int progressBarWidth = std::max(0, rect.width - panelPadding * 2);
  const int progressBarY = rect.y + rect.height - panelPadding - 10;
  const float elapsedSeconds = snapshot.samplerSampleRate == 0
      ? 0.0f
      : static_cast<float>(snapshot.samplerFrameCount) / static_cast<float>(snapshot.samplerSampleRate);
  const float totalSeconds = snapshot.samplerSampleRate == 0 || snapshot.samplerRecordMaxFrameCount == 0
      ? elapsedSeconds
      : static_cast<float>(snapshot.samplerRecordMaxFrameCount) / static_cast<float>(snapshot.samplerSampleRate);
  const std::string timeText = FormatSamplerSeconds(elapsedSeconds, true) + " / " +
                               FormatSamplerSeconds(std::max(totalSeconds, elapsedSeconds), false);

  DrawSamplerProgress(canvas,
                      MakeRect(rect.x + panelPadding, progressBarY, progressBarWidth, 6),
                      snapshot.samplerRecordProgress,
                      alpha);
  canvas.DrawTextInRect(MakeRect(rect.x + panelPadding,
                                 progressBarY - 12,
                                 progressBarWidth,
                                 10),
                        timeText,
                        text,
                        1,
                        ui::TextAlign::Right,
                        ui::VerticalAlign::Middle);
}

bool IsSamplerWaveformContext(const UiScreenSnapshot& snapshot)
{
  return snapshot.engineKind == SynthEngineKind::Sampler &&
         snapshot.modifier == UiModifier::Parameter &&
         (IsSamplerOverviewContext(snapshot) ||
          snapshot.samplerRecording ||
          snapshot.samplerRecordConfirmMode ||
          IsSamplerTrimWaveformFocus(snapshot.focusItemId));
}

void DrawSamplerParameterContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const bool recording = snapshot.samplerRecording;
  const bool recordConfirm = snapshot.samplerRecordConfirmMode;
  const bool parameterValuePage = false;

  if (IsSamplerOverviewContext(snapshot))
  {
    DrawSamplerOverviewContent(canvas, rect, snapshot, alpha);
    return;
  }

  canvas.DrawText(rect.x,
                  rect.y + 8,
                  TruncateText(canvas, (recording || recordConfirm) ? "Record" : SamplerWaveformTitle(snapshot), rect.width, 2),
                  accent,
                  2);

  if (parameterValuePage && !snapshot.secondaryText.empty())
  {
    canvas.DrawTextInRect(MakeRect(rect.x, rect.y + 8, rect.width, canvas.FontHeight(2)),
                          TruncateText(canvas, snapshot.secondaryText, rect.width, 2),
                          text,
                          2,
                          ui::TextAlign::Right,
                          ui::VerticalAlign::Top);
  }

  const int dividerY = rect.y + 30;
  canvas.DrawLine(rect.x, dividerY, rect.x + rect.width - 1, dividerY, divider);

  if (recordConfirm || recording)
  {
    DrawSamplerRecordPanel(canvas,
                           MakeRect(rect.x,
                                    dividerY + 8,
                                    rect.width,
                                    std::max(28, rect.height - (dividerY - rect.y) - 12)),
                           snapshot,
                           alpha);
    return;
  }

  const int waveformY = dividerY + 8;
  const int bottomReserve = recording ? 28 : 4;
  const int waveformHeight = std::max(28, rect.height - (waveformY - rect.y) - bottomReserve);
  const int waveformInsetX = recording ? 8 : 0;
  const ui::Rect waveformRect = MakeRect(rect.x + waveformInsetX,
                                         waveformY,
                                         std::max(0, rect.width - waveformInsetX * 2),
                                         waveformHeight);
  DrawSamplerWaveform(canvas, waveformRect, snapshot, alpha);

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
  const ui::Rect titleRect = MakeRect(rect.x + 4, rect.y + 34, rect.width - 8, 34);
  DrawMarqueeTextInRect(canvas,
                        titleRect,
                        snapshot.primaryText,
                        text,
                        3,
                        ui::TextAlign::Center);

  const ui::Rect looperRect = MakeRect(rect.x + 4, rect.y + rect.height - 11, rect.width - 8, 4);
  constexpr int kHomeKeyboardHeight = 42;
  const int keyboardTop = titleRect.y + titleRect.height +
                          std::max(2, (looperRect.y - (titleRect.y + titleRect.height) - kHomeKeyboardHeight) / 2) - 2;
  DrawMiniKeyboard(canvas, MakeRect(rect.x + 10, keyboardTop, rect.width - 20, kHomeKeyboardHeight), activeKeys, alpha);
  DrawThinLooperBar(canvas, looperRect, snapshot.looperProgress, alpha);
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

  if (IsSamplerWaveformContext(snapshot))
  {
    DrawSamplerParameterContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (snapshot.modifier == UiModifier::Sound)
  {
    DrawSoundContextContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (snapshot.showPresetBrowser)
  {
    DrawSoundContextContent(canvas, rect, snapshot, alpha);
    return;
  }

  if (IsKeyboardLayoutSnapshot(snapshot))
  {
    DrawKeyboardContextContent(canvas, rect, snapshot, alpha);
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

void DrawHelpPopup(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (!snapshot.showHelpPopup || alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Color background = FadeColor(kBackground, alpha);
  const ui::Color border = FadeColor(kAccent, alpha);
  const ui::Color text = FadeColor(kText, alpha);
  const ui::Color accent = FadeColor(kAccent, alpha);
  const ui::Color divider = FadeColor(kDivider, alpha);
  const int titleScale = 3;
  const int bodyScale = 2;
  const int horizontalPadding = 12;
  const int titleTop = 10;
  const int titleBottomGap = 8;
  const int bodyTopGap = 14;
  const int bodyBottomPadding = 16;

  const int popupWidth = rect.width;
  const int popupHeight = rect.height;
  const int popupX = rect.x;
  const int popupY = rect.y;
  const ui::Rect popup = MakeRect(popupX, popupY, popupWidth, popupHeight);

  canvas.FillRect(popup, background);
  canvas.DrawLine(popup.x, popup.y, popup.x + popup.width - 1, popup.y, border);
  canvas.DrawLine(popup.x, popup.y + popup.height - 1, popup.x + popup.width - 1, popup.y + popup.height - 1, border);
  canvas.DrawLine(popup.x, popup.y, popup.x, popup.y + popup.height - 1, border);
  canvas.DrawLine(popup.x + popup.width - 1, popup.y, popup.x + popup.width - 1, popup.y + popup.height - 1, border);
  const int titleY = popup.y + titleTop;
  const int dividerY = titleY + canvas.FontHeight(titleScale) + titleBottomGap;
  canvas.DrawText(popup.x + horizontalPadding,
                  titleY,
                  TruncateText(canvas, snapshot.helpPopupTitle, popup.width - horizontalPadding * 2, titleScale),
                  accent,
                  titleScale);
  canvas.DrawLine(popup.x + horizontalPadding,
                  dividerY,
                  popup.x + popup.width - horizontalPadding - 1,
                  dividerY,
                  divider);

  const ui::Rect bodyRect = MakeRect(popup.x + horizontalPadding,
                                     dividerY + bodyTopGap,
                                     popup.width - horizontalPadding * 2,
                                     std::max(0, popup.height - (dividerY - popup.y) - bodyTopGap - bodyBottomPadding));
  const int maxBodyLines = std::max(1, bodyRect.height / (canvas.FontHeight(bodyScale) + 2));
  canvas.DrawTextInRect(bodyRect,
                        WrapTextToWidth(canvas, snapshot.helpPopupText, bodyRect.width, bodyScale, maxBodyLines),
                        text,
                        bodyScale,
                        ui::TextAlign::Left,
                        ui::VerticalAlign::Top);
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
  DrawHelpPopup(canvas, MakeRect(0, 0, canvas.Width(), canvas.Height()), snapshot, alpha);
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