#include "Applications/Synth/UI/SynthUiRenderer.h"

#include "Applications/Synth/UI/Dx7/Dx7UiRenderer.h"
#include "Applications/Synth/UI/Juno/JunoUiRenderer.h"
#include "Applications/Synth/UI/Drums/DrumsUiRenderer.h"
#include "Applications/Synth/UI/Sequencer/SequencerUiRenderer.h"
#include "Applications/Synth/UI/Render/SynthUiRenderPrimitives.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>

namespace synth {

namespace {

using namespace ui_render;
using ActiveKeys = std::array<bool, kKeyboardMatrixRows * kKeyboardMatrixColumns>;

constexpr float kRecordingWaveformNoiseFloor = 0.02f;
constexpr float kRecordingWaveformDisplayThreshold = 0.08f;

bool IsKeyboardLayoutSnapshot(const UiScreenSnapshot& snapshot)
{
  return snapshot.mode == UiMode::ContextMenu && snapshot.modifier == UiModifier::Layout && snapshot.engineKind != SynthEngineKind::Drums;
}

bool SameText(std::string_view left, std::string_view right)
{
  return left.size() == right.size() &&
         std::equal(left.begin(), left.end(), right.begin(), [](char a, char b) {
           return std::toupper(static_cast<unsigned char>(a)) == std::toupper(static_cast<unsigned char>(b));
         });
}

std::string FormatSamplerSeconds(float seconds, bool fractional)
{
  const float clampedSeconds = std::max(0.0f, seconds);
  int wholeSeconds = static_cast<int>(std::floor(clampedSeconds));
  if (!fractional)
  {
    return std::to_string(wholeSeconds) + "S";
  }

  int tenths = static_cast<int>(std::floor((clampedSeconds - static_cast<float>(wholeSeconds)) * 10.0f + 0.5f));
  if (tenths >= 10)
  {
    ++wholeSeconds;
    tenths = 0;
  }

  return std::to_string(wholeSeconds) + "." + std::to_string(tenths) + "S";
}

float FramesToSeconds(std::uint32_t frames, std::uint32_t sampleRate)
{
  return sampleRate == 0 ? 0.0f : static_cast<float>(frames) / static_cast<float>(sampleRate);
}

// ---------------------------------------------------------------- Home

void DrawHomeContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, const ActiveKeys& activeKeys, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const ui::Rect body = InsetRect(rect, kHomeContentPadding, 0);
  // Keep the progress strip below the taller body labels.
  DrawLoopBar(canvas,
              MakeRect(body.x, canvas.Height() - kLoopBarHeight, body.width, kLoopBarHeight),
              snapshot.sequencer.patternCount,
              snapshot.sequencer.activePattern,
              snapshot.sequencer.patternProgress,
              snapshot.sequencer.playing,
              alpha);
  if (DrawSequencerHomeContent(canvas, rect, snapshot, alpha)) return;

  // Track page: instrument in cyan over the sound in grey; a shifted octave sits top right.
  DrawTitleBar(canvas, snapshot.title, snapshot.headerPrimary, kAccent, snapshot.primaryText, OctaveStatusText(snapshot),
               snapshot.keyboardOctaveChanged ? kAccent : kDim, alpha);

  // What is being played shows under the keyboard or pads.
  std::string held;
  if (snapshot.engineKind == SynthEngineKind::Drums)
  {
    held = DrawDrumsPads(canvas, body.x + (body.width - kKeyGridWidth) / 2, rect.y + 8, 62, activeKeys, alpha);
  }
  else
  {
    DrawMiniPiano(canvas, MakeRect(body.x + 8, rect.y + 10, body.width - 16, 50), snapshot, activeKeys, alpha);
    held = HeldNoteNames(snapshot, activeKeys);
  }
  const int heldScale = FitScale(canvas, held, body.width, 2);
  DrawMarqueeTextInRect(canvas, MakeRect(body.x, rect.y + 74, body.width, InkHeight(heldScale)), held, FadeColor(kAccent, alpha),
                        heldScale, ui::TextAlign::Center);
  if (held.empty())
    DrawInk(canvas, body.x, rect.y + 94, "HOME: SEQUENCER", FadeColor(kDim, alpha));
}

// ---------------------------------------------------------------- Sampler

bool IsSamplerOverviewContext(const UiScreenSnapshot& snapshot)
{
  return snapshot.engineKind == SynthEngineKind::Sampler &&
         snapshot.modifier == UiModifier::Parameter &&
         !snapshot.samplerRecording &&
         !snapshot.samplerRecordConfirmMode &&
         snapshot.focusItemId.empty();
}

bool IsSamplerTrimWaveformFocus(const std::string& focusItemId)
{
  return focusItemId == "par.sampler.group.trim" || focusItemId == "sample.start" || focusItemId == "sample.end";
}

bool IsSamplerWaveformContext(const UiScreenSnapshot& snapshot)
{
  return snapshot.engineKind == SynthEngineKind::Sampler &&
         snapshot.modifier == UiModifier::Parameter &&
         !snapshot.showPresetBrowser && snapshot.focusItemId != "par.sampler.group.edit" &&
         snapshot.focusItemId != "par.sampler.page.save" &&
         (IsSamplerOverviewContext(snapshot) ||
          snapshot.samplerRecording ||
          snapshot.samplerRecordConfirmMode ||
          IsSamplerTrimWaveformFocus(snapshot.focusItemId) ||
          (snapshot.focusItemId == "par.sampler.group.samp" && snapshot.samplerHasDraft));
}

void DrawSamplerWaveform(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, bool showTrim, float alpha)
{
  if (rect.width <= 0 || rect.height <= 0)
  {
    return;
  }

  const bool recording = snapshot.samplerRecording;
  const int midY = rect.y + rect.height / 2;
  canvas.FillRect(MakeRect(rect.x, midY, rect.width, 1), FadeColor(kRule, alpha));

  const std::uint32_t totalFrames = std::max<std::uint32_t>(1, snapshot.samplerFrameCount);
  const auto frameX = [&](std::uint32_t frame) {
    return rect.x + static_cast<int>((static_cast<std::uint64_t>(std::min(frame, totalFrames)) * static_cast<std::uint64_t>(rect.width - 1)) / totalFrames);
  };
  const bool hasTrim = !recording && snapshot.samplerFrameCount > 0;
  const int startX = hasTrim ? frameX(snapshot.samplerTrimStartFrame) : rect.x;
  const int endX = hasTrim ? frameX(snapshot.samplerTrimEndFrame) : rect.x + rect.width - 1;
  if (showTrim && hasTrim && endX > startX + 1)
  {
    canvas.FillRect(MakeRect(startX + 1, rect.y, endX - startX - 1, rect.height), FadeColor(kSurface, alpha));
  }

  // Audio inside the kept region is bright; trimmed-off audio stays visible but recedes.
  const std::size_t bins = snapshot.samplerWaveform.size();
  const ui::Color inside = FadeColor(recording ? kDanger : kAccent, alpha);
  const ui::Color outside = FadeColor(MixColor(kRule, kDim, 0.3f), alpha);
  for (std::size_t bin = 0; bin < bins; ++bin)
  {
    const int x0 = rect.x + static_cast<int>((bin * static_cast<std::size_t>(rect.width)) / bins);
    const int x1 = std::max(x0, rect.x + static_cast<int>(((bin + 1) * static_cast<std::size_t>(rect.width)) / bins) - 1);
    float peak = std::clamp(snapshot.samplerWaveform[bin], 0.0f, 1.0f);
    if (recording)
    {
      peak = std::clamp((peak - kRecordingWaveformNoiseFloor) / (1.0f - kRecordingWaveformNoiseFloor), 0.0f, 1.0f);
      peak = peak < kRecordingWaveformDisplayThreshold ? 0.0f : std::pow(peak, 0.55f);
    }
    if (peak <= 0.0f)
    {
      continue;
    }
    const int halfHeight = std::max(1, static_cast<int>(peak * static_cast<float>(rect.height / 2 - 1)));
    for (int x = x0; x <= x1; ++x)
    {
      const bool kept = !showTrim || !hasTrim || (x >= startX && x <= endX);
      canvas.FillRect(MakeRect(x, midY - halfHeight, 1, halfHeight * 2 + 1), kept ? inside : outside);
    }
  }

  if (!showTrim || !hasTrim)
  {
    return;
  }
  canvas.FillRect(MakeRect(startX, rect.y, 1, rect.height), FadeColor(kText, alpha));
  canvas.FillRect(MakeRect(endX, rect.y, 1, rect.height), FadeColor(kText, alpha));
  const std::uint32_t cursorFrame = snapshot.samplerPreviewing
      ? snapshot.samplerPreviewCursorFrame
      : static_cast<std::uint32_t>((static_cast<std::uint64_t>(snapshot.samplerTrimCursor) * snapshot.samplerFrameCount) / 127);
  canvas.FillRect(MakeRect(frameX(cursorFrame), rect.y + 2, 1, rect.height - 4),
                  FadeColor(snapshot.samplerPreviewing ? kPlayback : kDim, alpha));
}

void DrawSamplerRecordPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const float elapsed = FramesToSeconds(snapshot.samplerFrameCount, snapshot.samplerSampleRate);
  const float capacity = FramesToSeconds(snapshot.samplerRecordMaxFrameCount, snapshot.samplerSampleRate);
  const std::string limit = capacity >= 1.0f ? "MAX " + FormatSamplerSeconds(capacity, false) : std::string{};
  const bool inputError = snapshot.samplerRecordConfirmMode && snapshot.samplerStatusText == "INPUT ERR";

  if (inputError || (!snapshot.samplerRecording && snapshot.samplerHasDraft))
  {
    const int top = DrawPageHeader(canvas, rect, inputError ? "RECORD" : "REPLACE TAKE?", inputError ? "INPUT ERR" : "",
                                   inputError ? kAccent : kDanger, kDanger, alpha);
    DrawSubject(canvas, MakeRect(rect.x, top, rect.width, rect.height - top),
                inputError ? "NO INPUT" : "UNSAVED TAKE",
                inputError ? "CHECK THE MIC INPUT" : "STARTING A NEW TAKE DISCARDS IT", true, alpha);
    return;
  }

  const bool recording = snapshot.samplerRecording;
  // Before START the frame count still belongs to the loaded sample, so the timer reads zero.
  const int top = DrawPageHeader(canvas, rect, recording ? "RECORDING" : "RECORD", recording ? limit : std::string{},
                                 recording ? kDanger : kAccent, kDim, alpha);
  DrawInk(canvas, rect.x, top + 2, recording ? FormatSamplerSeconds(elapsed, true) : std::string{"0.0S"},
          FadeColor(recording ? kDanger : kDim, alpha), 4);
  if (recording)
  {
    DrawSamplerWaveform(canvas, MakeRect(rect.x, top + 32, rect.width, 36), snapshot, false, alpha);
    DrawMeter(canvas, MakeRect(rect.x, top + 76, rect.width, 2), snapshot.samplerRecordProgress, kDanger, alpha);
    return;
  }
  DrawInk(canvas, rect.x, top + 44, "INPUT", FadeColor(kDim, alpha), 1);
  const int inputWidth = InkWidth(canvas, "INPUT", 1) + 6;
  DrawMeter(canvas, MakeRect(rect.x + inputWidth, top + 46, rect.width - inputWidth, 5), snapshot.samplerInputLevel, kPlayback, alpha);
}

void DrawSamplerPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (snapshot.samplerRecording || snapshot.samplerRecordConfirmMode)
  {
    DrawSamplerRecordPage(canvas, rect, snapshot, alpha);
    return;
  }

  const float seconds = FramesToSeconds(snapshot.samplerFrameCount, snapshot.samplerSampleRate);
  if (snapshot.focusItemId == "par.sampler.group.samp")
  {
    // A finished take waits here for SAVE; the waveform confirms what was captured.
    const int top = DrawPageHeader(canvas, rect, "NEW TAKE", FormatSamplerSeconds(seconds, true), kAccent, kDim, alpha);
    DrawSamplerWaveform(canvas, MakeRect(rect.x, top + 4, rect.width, 64), snapshot, false, alpha);
    DrawInk(canvas, rect.x, top + 76, "NOT SAVED YET", FadeColor(kDim, alpha));
    return;
  }
  if (!IsSamplerOverviewContext(snapshot))
  {
    const float kept = FramesToSeconds(snapshot.samplerTrimEndFrame - std::min(snapshot.samplerTrimStartFrame, snapshot.samplerTrimEndFrame),
                                       snapshot.samplerSampleRate);
    const int top = DrawPageHeader(canvas, rect, "TRIM", FormatSamplerSeconds(kept, true), kAccent, kDim, alpha);
    DrawSamplerWaveform(canvas, MakeRect(rect.x, top + 4, rect.width, 64), snapshot, true, alpha);
    const int labelTop = top + 72;
    const int columnWidth = (rect.width - 6) / 2;
    const std::string start = FormatSamplerSeconds(FramesToSeconds(snapshot.samplerTrimStartFrame, snapshot.samplerSampleRate), true);
    const std::string end = FormatSamplerSeconds(FramesToSeconds(snapshot.samplerTrimEndFrame, snapshot.samplerSampleRate), true);
    const int endX = rect.x + columnWidth + 6;
    DrawInk(canvas, rect.x, labelTop, "START", FadeColor(kDim, alpha));
    DrawInk(canvas, endX, labelTop, "END", FadeColor(kDim, alpha));
    const int scale = std::min(FitScale(canvas, start, columnWidth, 2), FitScale(canvas, end, columnWidth, 2));
    DrawInk(canvas, rect.x, labelTop + 12, TruncateText(canvas, start, columnWidth, scale), FadeColor(kText, alpha), scale);
    DrawInk(canvas, endX, labelTop + 12, TruncateText(canvas, end, columnWidth, scale), FadeColor(kText, alpha), scale);
    return;
  }

  // The file and its length live in the title bar so the waveform can use the body.
  const bool hasAudio = snapshot.samplerHasSample || snapshot.samplerHasDraft;
  const std::string name = snapshot.samplerHasDraft ? (snapshot.samplerDraftName.empty() ? std::string{"NEW TAKE"} : snapshot.samplerDraftName)
                                                    : snapshot.samplerSampleName;
  const int top = DrawPageTitle(canvas, rect, "SAMPLER", hasAudio ? name : std::string{"EMPTY"}, kAccent,
                                hasAudio ? FormatSamplerSeconds(seconds, true) : std::string{}, snapshot.samplerHasDraft ? "UNSAVED" : "", alpha);
  if (!hasAudio)
  {
    canvas.FillRect(MakeRect(rect.x, top + 36, rect.width, 1), FadeColor(kRule, alpha));
    return;
  }
  DrawSamplerWaveform(canvas, MakeRect(rect.x, top + 6, rect.width, 60), snapshot, true, alpha);

  // Playback facts the keys below lead to: base note and one-shot or loop.
  const int factsTop = top + 76;
  const std::string root = NoteName(snapshot.samplerRoot);
  DrawInk(canvas, rect.x, factsTop, "ROOT", FadeColor(kDim, alpha));
  const int rootX = rect.x + InkWidth(canvas, "ROOT", 1) + 6;
  DrawInk(canvas, rootX, factsTop, root, FadeColor(kText, alpha));
  DrawInk(canvas, rootX + InkWidth(canvas, root, 1) + 8, factsTop, snapshot.samplerLoop != 0 ? "LOOP" : "ONE SHOT",
          FadeColor(kText, alpha));
}

// ---------------------------------------------------------------- Keyboard and drum pads

void DrawKeyboardPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const int top = DrawPageHeader(canvas, rect, snapshot.title, {}, kAccent, kDim, alpha);
  const bool disabled = (snapshot.focusItemId == "lay.root" && snapshot.keyboardRootDisabled) ||
                        (snapshot.focusItemId == "lay.scl" && snapshot.keyboardScaleDisabled);
  const std::string value = snapshot.secondaryText;
  DrawInk(canvas, rect.x, top + 6, TruncateText(canvas, value, rect.width, 2), FadeColor(disabled ? kDim : kText, alpha),
          FitScale(canvas, value, rect.width, 2));
  if (snapshot.focusItemId == "lay.fx")
  {
    return;
  }
  // The same 8x4 map as Home, so a layout change is visible before leaving the page.
  DrawPerformanceKeyMap(canvas, rect.x, top + 28, snapshot, ActiveKeys{}, alpha);
}

void DrawDrumPadsPage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const int top = DrawPageHeader(canvas, rect, snapshot.title, snapshot.meta, kAccent, kDim, alpha);
  DrawDrumsPads(canvas, rect.x, top + 8, 34, ActiveKeys{}, alpha);
  DrawInk(canvas, rect.x, top + 54, "PADS 1-8", FadeColor(kDim, alpha));
}

// ---------------------------------------------------------------- Shared templates

void DrawTemplatePage(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  const std::string title = !snapshot.title.empty() ? snapshot.title
                          : (!snapshot.primaryText.empty() ? snapshot.primaryText : snapshot.headerPrimary);
  const bool confirm = snapshot.contentStyle == UiContentStyle::Confirm;
  const bool dangerQuestion = confirm && snapshot.dangerous && !title.empty() && title.back() == '?';
  const bool transportPlaying = snapshot.focusItemId == "sys.seq" && snapshot.sequencer.playing;
  const ui::Color line1Color = snapshot.dangerous ? kDanger : (transportPlaying ? kPlayback : kAccent);
  // A search query takes the first line, with an underscore as the cursor.
  const bool searching = snapshot.contentStyle == UiContentStyle::List && !snapshot.searchText.empty();
  const std::string line1 = searching ? snapshot.searchText + "_" : snapshot.meta;
  const int top = DrawPageTitle(canvas, rect, title, line1, line1Color, snapshot.subtitle, snapshot.status, alpha);
  const ui::Rect body = MakeRect(rect.x, top, rect.width, rect.y + rect.height - top);

  switch (snapshot.contentStyle)
  {
    case UiContentStyle::List:
      DrawListRows(canvas, MakeRect(body.x, body.y + 2, body.width, body.height - 4), snapshot.secondaryText, alpha);
      return;
    case UiContentStyle::TextEntry:
      DrawTextEntry(canvas, body, snapshot.subject, alpha);
      return;
    case UiContentStyle::Confirm:
      DrawSubject(canvas, body, snapshot.subject, snapshot.note, dangerQuestion, alpha);
      return;
    case UiContentStyle::Adjust:
      DrawAdjustValue(canvas, body, SameText(snapshot.primaryText, title) ? std::string_view{} : std::string_view{snapshot.primaryText},
                      snapshot.secondaryText, snapshot.valueLevel, alpha);
      return;
    case UiContentStyle::KeyValues:
      DrawKeyValues(canvas, rect, snapshot.rightBarColumns, snapshot.rightBarItems, alpha);
      return;
    case UiContentStyle::Auto:
    default:
      if (snapshot.showSecondaryText && !snapshot.secondaryText.empty())
      {
        const std::size_t newline = snapshot.secondaryText.find('\n');
        std::string note = newline == std::string::npos ? std::string{} : snapshot.secondaryText.substr(newline + 1);
        std::replace(note.begin(), note.end(), '\n', ' ');
        DrawSubject(canvas, body, snapshot.secondaryText.substr(0, newline), note, false, alpha);
      }
      return;
  }
}

void DrawContextContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, float alpha)
{
  if (alpha <= 0.01f || rect.width <= 0 || rect.height <= 0)
  {
    return;
  }
  if (snapshot.modifier == UiModifier::Other && snapshot.focusItemId.rfind("sys.seq", 0) == 0 &&
      DrawSequencerContextContent(canvas, rect, snapshot, alpha))
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
    DrawSamplerPage(canvas, rect, snapshot, alpha);
    return;
  }
  if (snapshot.engineKind == SynthEngineKind::Drums && snapshot.modifier == UiModifier::Layout)
  {
    DrawDrumPadsPage(canvas, rect, snapshot, alpha);
    return;
  }
  if (IsKeyboardLayoutSnapshot(snapshot) && snapshot.contentStyle == UiContentStyle::Auto)
  {
    DrawKeyboardPage(canvas, rect, snapshot, alpha);
    return;
  }
  DrawTemplatePage(canvas, rect, snapshot, alpha);
}

// ---------------------------------------------------------------- Frame

ScreenLayout LayoutForSnapshot(const ui::Canvas& canvas, const UiScreenSnapshot& snapshot)
{
  ScreenLayout layout;
  const int leftWidth = BarWidthForColumns(snapshot.leftBarColumns, snapshot.leftBarWidthScale);
  layout.leftBar = MakeRect(snapshot.leftBarVisible ? 0 : -leftWidth, kTitleBarHeight, leftWidth, canvas.Height() - kTitleBarHeight);

  const int rightWidth = snapshot.rightBarVisible ? BarWidthForColumns(snapshot.rightBarColumns, snapshot.rightBarWidthScale) : 0;
  layout.rightBar = MakeRect(canvas.Width() - rightWidth, kTitleBarHeight, rightWidth, canvas.Height() - kTitleBarHeight);

  const int padding = snapshot.mode == UiMode::Home ? 0 : kContextContentPadding;
  const int contentLeft = (snapshot.leftBarVisible ? leftWidth : 0) + padding;
  const int contentRight = layout.rightBar.x - padding;
  layout.content = MakeRect(contentLeft, kTitleBarHeight, std::max(0, contentRight - contentLeft), canvas.Height() - kTitleBarHeight);
  return layout;
}

void DrawSnapshotContent(ui::Canvas& canvas, const ui::Rect& rect, const UiScreenSnapshot& snapshot, const ActiveKeys& activeKeys, float alpha)
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

  canvas.FillRect(rect, kBackground);
  const ui::Rect inner = InsetRect(rect, kContextContentPadding, 0);
  const int top = DrawPageHeader(canvas, inner, snapshot.helpPopupTitle, {}, kAccent, kDim, alpha);
  const std::string body = WrapTextToWidth(canvas, snapshot.helpPopupText, inner.width, 1, (rect.height - top - 6) / kListRowPitch);
  int y = top + 6;
  std::size_t start = 0;
  while (!body.empty() && start <= body.size())
  {
    const std::size_t newline = body.find('\n', start);
    DrawInk(canvas, inner.x, y, body.substr(start, newline == std::string::npos ? std::string::npos : newline - start), FadeColor(kText, alpha));
    if (newline == std::string::npos) break;
    start = newline + 1;
    y += kListRowPitch;
  }
}

void DrawSnapshotFrame(ui::Canvas& canvas,
                       const UiScreenSnapshot& snapshot,
                       const ScreenLayout& layout,
                       const ActiveKeys& activeKeys,
                       float alpha)
{
  DrawSegmentedBar(canvas, layout.leftBar, snapshot.leftBarColumns, snapshot.leftBarItems, BarAnchor::Left, alpha);
  DrawSegmentedBar(canvas, layout.rightBar, snapshot.rightBarColumns, snapshot.rightBarItems, BarAnchor::Right, alpha);
  DrawSnapshotContent(canvas, layout.content, snapshot, activeKeys, alpha);
  DrawHelpPopup(canvas, MakeRect(0, 0, canvas.Width(), canvas.Height()), snapshot, alpha);
}

void DrawTransitionFrame(ui::Canvas& canvas, const UiTransitionSnapshot& transition, const ActiveKeys& activeKeys)
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
