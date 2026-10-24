#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "Applications/Synth/UI/Groove/Render/Effects.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"
#include "Applications/Synth/UI/Groove/Render/Widgets.h"

namespace groove::render {

namespace {

using namespace groove::theme;
using widgets::Cell;
using widgets::CellSpan;
using widgets::CellStyle;

constexpr std::size_t kVisibleRows = 9;

void Title(Gfx& g, const std::string& title, Color color)
{
  g.TextFit(kContentX, kContentY, kContentWidth - 50, title, color);
}

void DrawList(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal, bool searchable)
{
  Title(g, modal.title, kTab);
  int listTop = kContentY + 12;
  if (searchable)
  {
    const int x = g.Text(kContentX + 60, kContentY, "/", kFn);
    const int end = g.Text(x + 2, kContentY, modal.search, kInk);
    if ((controller.State().now / 400) % 2 == 0) g.Fill(Rect{end, kContentY, 5, 7}, kInk);
    if (modal.search.empty()) g.Text(x + 10, kContentY, "TYPE TO SEARCH", kFaint);
  }
  char count[16];
  std::snprintf(count, sizeof(count), "%u/%u", static_cast<unsigned>(modal.items.empty() ? 0 : modal.cursor + 1), static_cast<unsigned>(modal.items.size()));
  g.Text(kContentRight - Gfx::Measure(count), kContentY, count, kDim);
  g.DottedHLine(kContentX, listTop - 2, kContentWidth, kFaint);
  if (modal.items.empty())
  {
    g.TextAligned(Rect{kContentX, listTop, kContentWidth, 60}, searchable && !modal.search.empty() ? "NO MATCH" : "EMPTY", kDim, Font::Micro, Align::Center);
    return;
  }
  const std::size_t first = modal.cursor >= kVisibleRows ? modal.cursor - kVisibleRows + 1 : 0;
  // The highlight glides between rows; rows type in when the list opens.
  const float barY = animator.Smooth(Animator::Key("list-cursor", static_cast<std::uint32_t>(modal.kind)),
                                     static_cast<float>(listTop + static_cast<int>(modal.cursor - first) * 10), 40.0f);
  g.Fill(Rect{kContentX, static_cast<int>(std::lround(barY)), kContentWidth - 6, 9}, kInk);
  const float opened = fx::Clamp01(static_cast<float>(animator.Now() - animator.ModalOpenedAt()) / 260.0f);
  for (std::size_t row = 0; row < kVisibleRows && first + row < modal.items.size(); ++row)
  {
    const std::size_t index = first + row;
    const auto& item = modal.items[index];
    const int y = listTop + 1 + static_cast<int>(row) * 10;
    const bool covered = std::fabs(static_cast<float>(y - 1) - barY) < 5.0f;
    const float reveal = fx::Clamp01(opened * 3.0f - static_cast<float>(row) * 0.25f);
    if (item.marked) g.Text(kContentX + 2, y, ">", covered ? kBlack : kTab);
    std::string label = item.label;
    label.resize(std::min(label.size(), static_cast<std::size_t>(reveal * static_cast<float>(label.size()) + 0.5f)));
    g.TextFit(kContentX + 10, y, kContentWidth - 18, label, covered ? kBlack : kInk);
  }
  widgets::ScrollIndicator(g, Rect{kContentRight - 2, listTop, 1, static_cast<int>(kVisibleRows) * 10}, first, kVisibleRows, modal.items.size());
}

void DrawTextEntry(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal)
{
  Title(g, modal.title, kTab);
  const Rect box{kContentX, kContentY + 30, kContentWidth, 24};
  g.DottedHLine(box.x, box.y + box.height, box.width, kDim);
  const int advance = Gfx::Advance(Font::Large);
  const std::size_t fit = static_cast<std::size_t>((box.width - advance) / advance);
  const std::string shown = modal.text.size() > fit ? modal.text.substr(modal.text.size() - fit) : modal.text;
  const int end = g.Text(box.x, box.y + 5, shown, kInk, Font::Large);
  const float pop = fx::Decay(animator.Now(), animator.TextTypedAt(), 220);
  if (pop > 0.0f && !shown.empty())
  {
    // The newest character lands with a flash and a ripple.
    const Rect last{end - advance, box.y + 3, advance, 18};
    if (pop > 0.6f) g.Fill(last, kTab);
    g.Text(end - advance, box.y + 5, shown.substr(shown.size() - 1), pop > 0.6f ? kBlack : kInk, Font::Large);
    fx::Ripple(g, last, 1.0f - pop, kTab);
  }
  if ((controller.State().now / 400) % 2 == 0) g.Fill(Rect{end + 1, box.y + 4, 10, 16}, kInk);
  char counter[16];
  std::snprintf(counter, sizeof(counter), "%u/%u", static_cast<unsigned>(modal.text.size()), static_cast<unsigned>(modal.maxLength));
  g.Text(kContentRight - Gfx::Measure(counter), kContentY, counter, kDim);
  g.Text(kContentX, kContentY + 70, "A-Z 0-9 SPACE -", kFaint);
}

void DrawConfirm(Gfx& g, Animator& animator, const Modal& modal)
{
  const Color accent = modal.danger ? kRec : kInk;
  g.TextFit(kContentX, kContentY + 14, kContentWidth, modal.title, kInk, Font::Small);
  g.TextFit(kContentX, kContentY + 30, kContentWidth, modal.detail, modal.danger ? kRec : kDim);
  const Rect yes{kContentX, kContentY + 56, 104, 26};
  const Rect no{kContentX + 115, kContentY + 56, 104, 26};
  g.Fill(yes, accent);
  if (modal.danger)
  {
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(animator.Now()) / 120.0f);
    fx::Ripple(g, yes, 0.2f + pulse * 0.5f, kRec);
  }
  g.DrawGlyph(yes.x + 8, yes.y + 10, Glyph::Enter, kBlack);
  g.Text(yes.x + 20, yes.y + 9, "ENTER  YES", kBlack, Font::Micro);
  g.Stroke(no, kDim);
  g.Text(no.x + 20, no.y + 9, "ESC  NO", kInk, Font::Micro);
}

void DrawEnginePicker(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal)
{
  Title(g, "ENGINE  T" + std::to_string(controller.State().track + 1), kTab);
  static constexpr std::array<const char*, 4> kDetails{{"6-OPERATOR FM", "ANALOG POLY", "SAMPLE PLAYER", "808 DRUM KIT"}};
  for (std::size_t index = 0; index < modal.items.size() && index < 4; ++index)
  {
    const Rect cell = Cell(1, index);
    const bool selected = index == modal.cursor;
    const auto& item = modal.items[index];
    if (selected)
    {
      g.Fill(cell, kInk);
      fx::MarchingAnts(g, Rect{cell.x - 2, cell.y - 2, cell.width + 4, cell.height + 4}, kTab, animator.Now());
    }
    else g.Stroke(cell, kFaint);
    g.TextFit(cell.x + 3, cell.y + 3, cell.width - 5, item.label, selected ? kBlack : kInk, Font::Small);
    if (item.marked) g.Text(cell.x + 3, cell.y + 15, "NOW", selected ? kBlack : kTab);
  }
  if (modal.cursor < kDetails.size())
    fx::DecodeText(g, Cell(2, 0).x, Cell(2, 0).y + 6, kDetails[modal.cursor], kDim, Font::Micro,
                   fx::Progress(animator.Now(), animator.ModalOpenedAt(), 400), static_cast<std::uint32_t>(modal.cursor) + animator.Now() / 50);
  g.Text(Cell(3, 0).x, Cell(3, 0).y + 6, "THE TRACK SOUND IS REPLACED", kFaint);
}

void DrawSampleRecord(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const auto sampler = host.Sampler().BuildUiState(state.track);
  const auto* engine = host.Engine(state.track);
  const double maxSeconds = sampler.sampleRate > 0 ? static_cast<double>(sampler.recordMaxFrameCount) / sampler.sampleRate : 5.0;
  if (sampler.recording)
  {
    const bool blink = (state.now / 300) % 2 == 0;
    const float breath = 0.5f + 0.5f * std::sin(static_cast<float>(animator.Now()) / 110.0f);
    g.Dither(Rect{kContentX - 2, kContentY + 2, 9, 11}, kRec, static_cast<int>(breath * 8.0f));
    g.DrawGlyph(kContentX, kContentY + 4, Glyph::Rec, blink ? kRec : kFaint);
    g.Text(kContentX + 10, kContentY + 2, "RECORDING", kRec, Font::Small);
    char time[24];
    std::snprintf(time, sizeof(time), "%.1f / %.1fS", static_cast<double>(sampler.recordProgress) * maxSeconds, maxSeconds);
    g.Text(kContentX, kContentY + 22, time, kInk, Font::Large);
    widgets::LevelBar(g, Rect{kContentX, kContentY + 46, kContentWidth, 6}, sampler.recordProgress, kRec, kFaint);
    g.Text(kContentX, kContentY + 62, "INPUT", kDim);
    widgets::LevelBar(g, Rect{kContentX + 36, kContentY + 63, kContentWidth - 36, 5}, sampler.inputLevel, sampler.inputLevel > 0.95f ? kRec : kPlay, kFaint);
    return;
  }
  if (sampler.hasDraft)
  {
    Title(g, "TAKE", kTab);
    char length[24];
    std::snprintf(length, sizeof(length), "%.2fS", sampler.sampleRate > 0 ? static_cast<double>(sampler.frameCount) / sampler.sampleRate : 0.0);
    g.Text(kContentRight - Gfx::Measure(length), kContentY, length, kDim);
    synth::SynthParameterInfo start, end;
    const bool hasStart = engine != nullptr && engine->GetParameterInfo("sample.start", start);
    const bool hasEnd = engine != nullptr && engine->GetParameterInfo("sample.end", end);
    const Modal* modal = controller.TopModal();
    const std::uint8_t marker = modal != nullptr ? modal->trimMarker : 0;
    widgets::ParamCell(g, Cell(0, 0), "START", hasStart ? std::to_string(start.value) : "--", hasStart ? start.value / 127.0f : -1.0f,
                       marker == 0 ? CellStyle::Focused : CellStyle::Normal);
    widgets::ParamCell(g, Cell(0, 1), "END", hasEnd ? std::to_string(end.value) : "--", hasEnd ? end.value / 127.0f : -1.0f,
                       marker == 1 ? CellStyle::Focused : CellStyle::Normal);
    widgets::ActionCell(g, Cell(0, 2), "PLAY", state.gridDown[2], kPlay);
    widgets::ActionCell(g, Cell(0, 3), "SAVE", state.gridDown[3], kTab);
    const Rect wave = CellSpan(1, 0, 3, 4);
    const float cursor = sampler.frameCount > 0 ? static_cast<float>(sampler.previewCursorFrame) / static_cast<float>(sampler.frameCount) : 0.0f;
    widgets::Waveform(g, Rect{wave.x, wave.y + 4, wave.width, wave.height - 4}, sampler.waveform.data(), sampler.waveform.size(),
                      hasStart ? start.value / 127.0f : 0.0f, hasEnd ? end.value / 127.0f : 1.0f, cursor, sampler.previewing);
    return;
  }
  Title(g, "RECORD SAMPLE  T" + std::to_string(state.track + 1), kTab);
  g.Text(kContentX, kContentY + 22, "READY", kInk, Font::Large);
  char limit[24];
  std::snprintf(limit, sizeof(limit), "MIC  %.1fS MAX", maxSeconds);
  g.Text(kContentX, kContentY + 44, limit, kDim);
  g.Text(kContentX, kContentY + 62, "INPUT", kDim);
  widgets::LevelBar(g, Rect{kContentX + 36, kContentY + 63, kContentWidth - 36, 5}, sampler.inputLevel, kPlay, kFaint);
  if (sampler.phase == synth::SamplerTrackPhase::Error) g.TextFit(kContentX, kContentY + 78, kContentWidth, sampler.statusText, kRec);
}

}  // namespace

void ModalContent(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal)
{
  switch (modal.kind)
  {
    case ModalKind::TextEntry: DrawTextEntry(g, controller, animator, modal); break;
    case ModalKind::Confirm: DrawConfirm(g, animator, modal); break;
    case ModalKind::PresetBrowser:
    case ModalKind::SampleBrowser: DrawList(g, controller, animator, modal, true); break;
    case ModalKind::SampleOptions: DrawList(g, controller, animator, modal, false); break;
    case ModalKind::EnginePicker: DrawEnginePicker(g, controller, animator, modal); break;
    case ModalKind::SampleRecord: DrawSampleRecord(g, controller, animator); break;
    case ModalKind::None:
    default: break;
  }
}

const char* ModalHint(const Controller& controller, const Modal& modal)
{
  switch (modal.kind)
  {
    case ModalKind::TextEntry: return "ENTER OK   DEL ERASE   ESC CANCEL";
    case ModalKind::Confirm: return modal.danger ? "ENTER CONFIRM   ESC KEEP" : "ENTER YES   ESC NO";
    case ModalKind::PresetBrowser: return "SUB ADD MOVE   ENTER LOAD   ESC BACK";
    case ModalKind::SampleBrowser: return "ENTER LOAD   FN+ENTER RENAME/DELETE";
    case ModalKind::SampleOptions: return "ENTER CHOOSE   ESC BACK";
    case ModalKind::EnginePicker: return "GRID OR SUB ADD   ENTER SWITCH";
    case ModalKind::SampleRecord:
    {
      const auto sampler = controller.HostView().Sampler().BuildUiState(controller.State().track);
      if (sampler.recording) return "ENTER STOP   ESC CANCEL";
      if (sampler.hasDraft) return "SUB ADD TRIM   ENTER START/END   ESC DISCARD";
      return "ENTER RECORD   ESC CLOSE";
    }
    case ModalKind::None:
    default: return "";
  }
}

}  // namespace groove::render
