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
    if (modal.search.empty()) g.Text(x + 10, kContentY, "FN+F SEARCH", kFaint);
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
  // The window of text keeps the cursor in view.
  const std::size_t cursor = std::min(modal.textCursor, modal.text.size());
  const std::size_t first = cursor > fit ? cursor - fit : 0;
  const std::string shown = modal.text.substr(first, fit);
  g.Text(box.x, box.y + 5, shown, kInk, Font::Large);
  const int caretX = box.x + static_cast<int>(cursor - first) * advance;
  const float pop = fx::Decay(animator.Now(), animator.TextTypedAt(), 220);
  if (pop > 0.0f && cursor > first)
  {
    // The character just typed lands with a flash and a ripple.
    const Rect last{caretX - advance, box.y + 3, advance, 18};
    if (pop > 0.6f) g.Fill(last, kTab);
    g.Text(caretX - advance, box.y + 5, modal.text.substr(cursor - 1, 1), pop > 0.6f ? kBlack : kInk, Font::Large);
    fx::Ripple(g, last, 1.0f - pop, kTab);
  }
  // A block at the end, a bar between characters.
  if ((controller.State().now / 400) % 2 == 0)
    g.Fill(cursor < modal.text.size() ? Rect{caretX - 1, box.y + 2, 2, 20} : Rect{caretX + 1, box.y + 4, 10, 16}, kInk);
  char counter[24];
  std::snprintf(counter, sizeof(counter), "%u/%u", static_cast<unsigned>(modal.text.size()), static_cast<unsigned>(modal.maxLength));
  g.Text(kContentRight - Gfx::Measure(counter), kContentY, counter, kDim);
  g.Text(kContentX, kContentY + 70, "A-Z 0-9 SPACE -", kFaint);
  g.Text(kContentX, kContentY + 80, "FN + , / MOVE   FN + ; . START / END   FN + DEL DELETE", kFaint);
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

// RECORD (hold OK in SEQ): the record settings on row 0, the captured takes on rows 1 and 2 (newest
// first) and below them the bar of the chosen take that OK would put into a pattern.
void DrawRecord(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const RecordState& rec = state.record;
  const std::uint32_t now = animator.Now();
  const bool armed = controller.HostView().Sequencer().RecordArmed();
  const std::array<std::pair<const char*, std::string>, 4> settings{{
      {"REC", armed ? "ON" : "OFF"},
      {"SNAP", rec.snap ? "STEPS" : "FREE"},
      {"STEP", "+" + std::to_string(rec.stepLength)},
      {"VIEW", rec.view == SeqView::Roll ? "ROLL" : "STEP"},
  }};
  for (std::size_t index = 0; index < settings.size(); ++index)
  {
    const CellStyle style = rec.focus == index ? CellStyle::Focused : CellStyle::Normal;
    widgets::ParamCell(g, Cell(0, index), settings[index].first, settings[index].second, -1.0f, style);
    if (index == kRecordRec && armed && (now / 400) % 2) g.Fill(Rect{Cell(0, index).x + Cell(0, index).width - 7, Cell(0, index).y + 3, 4, 4}, kRec);
  }
  for (std::size_t order = 0; order < kCaptureTakes; ++order)
  {
    const std::size_t slot = kRecordFirstTake + order;
    const Rect cell = Cell(slot / kGridColumns, slot % kGridColumns);
    const CaptureTake* take = controller.TakeAt(order);
    if (take == nullptr)
    {
      g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kShade);
      if (order == 0) g.Text(cell.x + 2, cell.y + 3, "PLAY PADS", kFaint, Font::Micro);
      continue;
    }
    const bool focused = rec.focus == slot;
    const Color color = TrackColor(take->track);
    if (focused) g.Fill(cell, kInk);
    const Color ink = focused ? kBlack : kInk;
    char label[16];
    std::snprintf(label, sizeof(label), "#%u", static_cast<unsigned>(take->id));
    g.Fill(Rect{cell.x, cell.y, 3, cell.height}, color);
    g.Text(cell.x + 5, cell.y + 2, label, ink);
    g.Text(cell.x + cell.width - Gfx::Measure("T4") - 2, cell.y + 2, "T" + std::to_string(take->track + 1), focused ? kBlack : color);
    const std::string info = std::to_string(take->count) + (take->count == 1 ? " NOTE" : " NOTES");
    g.TextFit(cell.x + 5, cell.y + 12, cell.width - 6, info, focused ? kBlack : kDim, Font::Micro);
    // A sketch of the take: its notes as ticks over time.
    const std::uint32_t span = std::max<std::uint32_t>(1, take->endedAt - take->startedAt);
    for (std::size_t index = 0; index < take->count; ++index)
    {
      const auto& note = take->notes[index];
      const int x = cell.x + 5 + static_cast<int>(static_cast<std::uint64_t>(note.startMs) * static_cast<std::uint64_t>(cell.width - 8) / span);
      g.Pixel(x, cell.y + cell.height - 3 - (note.note % 12) / 4, focused ? kBlack : color);
    }
  }
  // Row 3: what OK does now.
  const Rect area{Cell(3, 0).x, Cell(3, 0).y, Cell(3, 3).x + Cell(3, 3).width - Cell(3, 0).x, Cell(3, 0).height};
  const CaptureTake* take = rec.focus >= kRecordFirstTake ? controller.TakeAt(rec.focus - kRecordFirstTake) : nullptr;
  if (take == nullptr)
  {
    static constexpr std::array<const char*, 4> kHelp{{
        "REC: PLAY + PADS RECORD INTO THE PATTERN (ALSO FN + PLAY)",
        "SNAP: TAKES LAND ON WHOLE STEPS, OR KEEP THEIR TIMING",
        "STEP REC: STEPS THE CURSOR MOVES AFTER EACH WRITE",
        "VIEW: PIANO ROLL OR STEP REC (TAP OK)",
    }};
    g.TextFit(area.x + 2, area.y + 4, area.width - 4, rec.focus < kHelp.size() ? kHelp[rec.focus] : "", kDim, Font::Micro);
    g.TextFit(area.x + 2, area.y + 15, area.width - 4,
              controller.TakeCount() == 0 ? "EVERYTHING THE PADS PLAY IS KEPT HERE AS TAKES" : "PICK A TAKE ABOVE TO PUT IT INTO A PATTERN",
              kFaint, Font::Micro);
    return;
  }
  const std::size_t bars = controller.TakeBars(*take);
  const std::size_t bar = std::min<std::size_t>(rec.takeBar, bars - 1);
  std::array<Controller::PlacedNote, kCaptureNotes> notes{};
  const std::size_t count = controller.TakeBarNotes(*take, bar, notes);
  const std::size_t steps = std::max<std::size_t>(1, controller.HostView().Sequencer().TrackStepCount(take->track));
  const int labelWidth = 44;
  char barLabel[24];
  std::snprintf(barLabel, sizeof(barLabel), "BAR %u/%u", static_cast<unsigned>(bar + 1), static_cast<unsigned>(bars));
  g.Text(area.x + 2, area.y + 3, barLabel, kInk);
  const std::uint32_t age = (now - take->endedAt) / 1000;
  g.Text(area.x + 2, area.y + 13, age < 60 ? std::to_string(age) + "S AGO" : std::to_string(age / 60) + "M AGO", kFaint, Font::Micro);
  const Rect lane{area.x + labelWidth, area.y + 2, area.width - labelWidth - 2, area.height - 4};
  g.Stroke(lane, kShade);
  int low = 127, high = 0;
  for (std::size_t index = 0; index < count; ++index)
  {
    low = std::min<int>(low, notes[index].note);
    high = std::max<int>(high, notes[index].note);
  }
  const int span = std::max(1, high - low);
  for (std::size_t index = 0; index < count; ++index)
  {
    const auto& note = notes[index];
    const int x = lane.x + 1 + static_cast<int>((note.step * synth::kSequencerPulsesPerStep + note.microstep * synth::kSequencerPulsesPerMicrostep) *
                                                (lane.width - 2) / (steps * synth::kSequencerPulsesPerStep));
    const int width = std::max(2, static_cast<int>(note.lengthPulses * (lane.width - 2) / (steps * synth::kSequencerPulsesPerStep)));
    const int y = lane.y + lane.height - 3 - (note.note - low) * (lane.height - 4) / span;
    g.Fill(Rect{x, y, std::min(width, lane.x + lane.width - 1 - x), 2}, TrackColor(take->track));
  }
  if (count == 0) g.TextAligned(lane, "NO NOTES IN THIS BAR", kFaint, Font::Micro, Align::Center);
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
    case ModalKind::Menu: MenuContent(g, controller, animator, 1.0f); break;
    case ModalKind::Record: DrawRecord(g, controller, animator); break;
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
    case ModalKind::PresetBrowser: return "ENTER LOAD   FN+F SEARCH   ESC BACK";
    case ModalKind::SampleBrowser: return "ENTER LOAD  FN+F SEARCH  FN+ENTER EDIT";
    case ModalKind::SampleOptions: return "ENTER CHOOSE   ESC BACK";
    case ModalKind::EnginePicker: return "GRID OR SUB ADD   ENTER SWITCH";
    case ModalKind::Menu: return "GRID RUNS   FN OR ESC CLOSES";
    case ModalKind::Record: return "+- BAR  OK NEW PATTERN  FN+OK MERGE";
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
