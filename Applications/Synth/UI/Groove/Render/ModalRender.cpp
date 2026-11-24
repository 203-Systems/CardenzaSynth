#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>

#include "Applications/Synth/UI/Groove/Render/Effects.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"
#include "Applications/Synth/UI/Groove/Render/Widgets.h"

namespace groove::render {

namespace {

using namespace groove::theme;
using widgets::Cell;
using widgets::CellSpan;
using widgets::CellStyle;


void DrawList(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal, bool searchable)
{
  const int listTop = kContentY + 12;
  if (searchable)
  {
    const int x = g.Text(kContentX, kContentY, "/", kFn);
    const int end = g.Text(x + 2, kContentY, modal.search, kInk);
    if ((controller.State().now / 400) % 2 == 0) g.Fill(Rect{end, kContentY, 5, 7}, kInk);
    if (modal.search.empty())
      g.Text(x + 10, kContentY, modal.kind == ModalKind::PresetBrowser ? "TYPE TO SEARCH" : "FN+F SEARCH", kFaint);
    char count[16];
    std::snprintf(count, sizeof(count), "%u/%u", static_cast<unsigned>(modal.Count() == 0 ? 0 : modal.cursor + 1), static_cast<unsigned>(modal.Count()));
    g.Text(kContentRight - Gfx::Measure(count), kContentY, count, kDim);
  }
  else
  {
    g.TextFit(kContentX, kContentY, kContentWidth, modal.title, kInk);
  }
  g.DottedHLine(kContentX, listTop - 2, kContentWidth, kFaint);
  if (modal.Count() == 0)
  {
    g.TextAligned(Rect{kContentX, listTop, kContentWidth, 60}, searchable && !modal.search.empty() ? "NO MATCH" : "EMPTY", kDim, Font::Micro, Align::Center);
    return;
  }
  const std::size_t first = ListFirstRow(modal.cursor);
  // The highlight glides between rows; rows type in when the list opens.
  const float barY = animator.Smooth(Animator::Key("list-cursor", static_cast<std::uint32_t>(modal.kind)),
                                     static_cast<float>(listTop + static_cast<int>(modal.cursor - first) * 10), 40.0f);
  g.Fill(Rect{kContentX, static_cast<int>(std::lround(barY)), kContentWidth - 6, 9}, kInk);
  const float opened = fx::Clamp01(static_cast<float>(animator.Now() - animator.ModalOpenedAt()) / 260.0f);
  // The preset browser: the row the held pads play (the preset on trial, or the track's own) has a
  // play mark at its end.
  const std::string heard = modal.kind == ModalKind::PresetBrowser ? controller.PresetHeard() : std::string();
  for (std::size_t row = 0; row < kListVisibleRows && first + row < modal.Count(); ++row)
  {
    const ListItem* listed = modal.Row(first + row);
    if (listed == nullptr) continue;
    const auto& item = *listed;
    const int y = listTop + 1 + static_cast<int>(row) * 10;
    const bool covered = std::fabs(static_cast<float>(y - 1) - barY) < 5.0f;
    const float reveal = fx::Clamp01(opened * 3.0f - static_cast<float>(row) * 0.25f);
    if (item.marked) g.Text(kContentX + 2, y, ">", covered ? kBlack : kTab);
    const bool playing = !heard.empty() && item.id == heard;
    if (playing) g.DrawGlyph(kContentRight - 14, y, Glyph::Play, covered ? kBlack : kPlay);
    std::string label = item.label;
    label.resize(std::min(label.size(), static_cast<std::size_t>(reveal * static_cast<float>(label.size()) + 0.5f)));
    g.TextFit(kContentX + 10, y, kContentWidth - (playing ? 27 : 18), label, covered ? kBlack : kInk);
  }
  widgets::ScrollIndicator(g, Rect{kContentRight - 2, listTop, 1, static_cast<int>(kListVisibleRows) * 10}, first, kListVisibleRows, modal.Count());
}

void DrawTextEntry(Gfx& g, const Controller& controller, Animator& animator, const Modal& modal)
{
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
  g.Text(kContentX, kContentY + 70, "A-Z 0-9 SPACE -   DEL ERASE", kFaint);
  g.Text(kContentX, kContentY + 80, "FN + , / MOVE   FN + ; . START / END   FN + DEL DELETE", kFaint);
}

// A confirm or a notice keeps clear of the edge strips: its words and buttons sit this far inside the
// content area on both sides, and its two buttons share the width between.
constexpr int kPanelPad = 6;
constexpr int kPanelX = kContentX + kPanelPad;
constexpr int kPanelWidth = kContentWidth - 2 * kPanelPad;
constexpr int kButtonGap = 10;
constexpr int kButtonWidth = (kPanelWidth - kButtonGap) / 2;

// A notice: the title, the first line of the detail as the warning it is, the rest dim, and one way out.
void DrawNotice(Gfx& g, const Modal& modal)
{
  g.TextFit(kPanelX, kContentY + 14, kPanelWidth, modal.title, kInk, Font::Small);
  std::string_view detail = modal.detail;
  for (int line = 0; !detail.empty() && line < 2; ++line)
  {
    const std::size_t end = detail.find('\n');
    g.TextFit(kPanelX, kContentY + 30 + line * 10, kPanelWidth, detail.substr(0, end), line == 0 ? kRec : kDim);
    detail = end == std::string_view::npos ? std::string_view() : detail.substr(end + 1);
  }
  const Rect ok{kPanelX, kContentY + 56, kButtonWidth, 26};
  g.Fill(ok, kInk);
  g.DrawGlyph(ok.x + 8, ok.y + 10, Glyph::Enter, kBlack);
  g.Text(ok.x + 20, ok.y + 9, "ENTER  OK", kBlack, Font::Micro);
}

void DrawConfirm(Gfx& g, Animator& animator, const Modal& modal)
{
  if (modal.confirmPurpose == ConfirmPurpose::Notice)
  {
    DrawNotice(g, modal);
    return;
  }
  const Color accent = modal.danger ? kRec : kInk;
  g.TextFit(kPanelX, kContentY + 14, kPanelWidth, modal.title, kInk, Font::Small);
  g.TextFit(kPanelX, kContentY + 30, kPanelWidth, modal.detail, modal.danger ? kRec : kDim);
  const Rect yes{kPanelX, kContentY + 56, kButtonWidth, 26};
  const Rect no{kPanelX + kButtonWidth + kButtonGap, kContentY + 56, kButtonWidth, 26};
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

void DrawEnginePicker(Gfx& g, Animator& animator, const Modal& modal)
{
  static constexpr std::array<const char*, 4> kDetails{{"6-OPERATOR FM", "ANALOG POLY", "SAMPLE PLAYER", "808 DRUM KIT"}};
  for (std::size_t index = 0; index < modal.items.size() && index < 4; ++index)
  {
    const Rect cell = Cell(0, index);
    const bool selected = index == modal.cursor;
    const auto& item = modal.items[index];
    if (selected)
    {
      g.Fill(cell, kInk);
      fx::MarchingAnts(g, Rect{cell.x - 2, cell.y - 2, cell.width + 4, cell.height + 4}, kTab, animator.Now());
    }
    else g.Stroke(cell, kFaint);
    g.TextFit(cell.x + 3, cell.y + 3, cell.width - 5, item.label, selected ? kBlack : kInk, Font::Small);
    if (item.marked) g.DrawGlyph(cell.x + 3, cell.y + 15, Glyph::Check, selected ? kBlack : kTab);
  }
  if (modal.cursor < kDetails.size())
    fx::DecodeText(g, Cell(1, 0).x, Cell(1, 0).y + 6, kDetails[modal.cursor], kDim, Font::Micro,
                   fx::Progress(animator.Now(), animator.ModalOpenedAt(), 400), static_cast<std::uint32_t>(modal.cursor) + animator.Now() / 50);
  g.Text(Cell(2, 0).x, Cell(2, 0).y + 6, "THE TRACK SOUND IS REPLACED", kFaint);
}

// SAMPLE RECORD keeps one layout in its three states. Row 0 is what the GRID's top row does: record
// or stop, the gain ADD / SUB change and the time; with a take START, END, PLAY and SAVE. Rows 1-3
// show the sound: the last moments while recording, the take after.

// GRID 1 while there is no take: it records, and stops (on a red dither) while recording.
void RecordKeyCell(Gfx& g, const Rect& cell, bool recording, bool pressed)
{
  const int y = cell.y + (cell.height - 7) / 2;
  if (pressed) g.Fill(cell, kRec);
  else if (recording)
  {
    g.Dither(cell, kRec, kDitherFill);
    g.Stroke(cell, kRec);
  }
  else
  {
    g.DottedHLine(cell.x, cell.y, cell.width, kFaint);
    g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
    g.DottedVLine(cell.x, cell.y, cell.height, kFaint);
    g.DottedVLine(cell.x + cell.width - 1, cell.y, cell.height, kFaint);
  }
  const Color ink = pressed ? kBlack : kInk;
  if (recording)
  {
    g.Fill(Rect{cell.x + 6, y, 7, 7}, ink);
    g.Text(cell.x + 17, y, "STOP", ink);
  }
  else
  {
    g.DrawGlyph(cell.x + 6, y, Glyph::Rec, pressed ? kBlack : kRec);
    g.Text(cell.x + 17, y, "REC", ink);
  }
}

// A time with its unit set small, so 1.3S never reads as 1.35.
void TimeText(Gfx& g, int x, int y, double seconds, Color color)
{
  char number[16];
  std::snprintf(number, sizeof(number), "%.1f", seconds);
  const int end = g.Text(x, y, number, color, Font::Large);
  g.Text(end + 2, y + 5, "S", color == kInk ? kDim : color, Font::Small);
}

// The meter reads the newest bins of the scrolling waveform: a read of the microphone that found
// nothing new does not drop it to zero.
float RecentLevel(const synth::SamplerTrackUiState& sampler)
{
  const auto& bins = sampler.waveform;
  return std::max({bins[bins.size() - 1], bins[bins.size() - 2], bins[bins.size() - 3]});
}

void InputMeter(Gfx& g, const Rect& rect, float level)
{
  const int filled = static_cast<int>(std::lround(fx::Clamp01(level) * static_cast<float>(rect.height)));
  g.Dither(Rect{rect.x, rect.y, rect.width, rect.height - filled}, kFaint, 4);
  g.Dither(Rect{rect.x, rect.y + rect.height - filled, rect.width, filled}, level > 0.95f ? kRec : kPlay, kDitherLevelOn);
  g.HLine(rect.x - 2, rect.y, rect.width + 2, kDim);  // full scale
}

// The waveform's bins as bars: the last three quarters of a second while recording (newest on the
// right, clipped bars red), the whole take after it, the part START / END leave out dim.
void Bars(Gfx& g, const Rect& area, const synth::SamplerTrackUiState& sampler, float start, float end, bool live)
{
  const int mid = area.y + area.height / 2;
  g.DottedHLine(area.x, mid, area.width, kFaint, 3);
  const auto& bins = sampler.waveform;
  const int count = static_cast<int>(bins.size());
  const int pitch = area.width / count;
  const int left = live ? area.x + area.width - pitch * count : area.x + (area.width - pitch * count) / 2;
  const int span = pitch * count - 2;
  const int startX = left + static_cast<int>(fx::Clamp01(start) * static_cast<float>(span));
  const int endX = left + static_cast<int>(fx::Clamp01(end) * static_cast<float>(span));
  for (int index = 0; index < count; ++index)
  {
    const float value = fx::Clamp01(bins[static_cast<std::size_t>(index)]);
    const int half = static_cast<int>(value * static_cast<float>(area.height / 2 - (live ? 1 : 3)));
    if (half <= 0) continue;
    const int x = left + index * pitch;
    const bool inside = live || (x + pitch - 2 > startX && x < endX);
    const Color color = !inside ? kFaint : live && value >= 0.98f ? kRec : kInk;
    g.Dither(Rect{x, mid - half, pitch - 2, half * 2 + 1}, color, inside ? kDitherLevelOn : 8);
  }
  if (live) return;
  g.VLine(startX, area.y, area.height, kInk);
  g.Fill(Rect{startX, area.y, 4, 3}, kInk);
  g.VLine(endX, area.y, area.height, kInk);
  g.Fill(Rect{endX - 3, area.y + area.height - 3, 4, 3}, kInk);
}

void DrawSampleRecord(Gfx& g, const Controller& controller)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const auto& manager = host.Sampler();
  const auto sampler = manager.BuildUiState(state.track);
  const Rect panel = CellSpan(1, 0, 3, 4);
  if (sampler.hasDraft)
  {
    const auto* engine = host.Engine(state.track);
    synth::SynthParameterInfo start, end;
    const bool hasStart = engine != nullptr && engine->GetParameterInfo("sample.start", start);
    const bool hasEnd = engine != nullptr && engine->GetParameterInfo("sample.end", end);
    const Modal* modal = controller.TopModal();
    const std::uint8_t marker = modal != nullptr ? modal->trimMarker : 0;
    widgets::ParamCell(g, Cell(0, 0), "START", hasStart ? engine->DescribeParameterValue("sample.start") : "--", hasStart ? start.value / 127.0f : -1.0f,
                       marker == 0 ? CellStyle::Focused : CellStyle::Normal);
    widgets::ParamCell(g, Cell(0, 1), "END", hasEnd ? engine->DescribeParameterValue("sample.end") : "--", hasEnd ? end.value / 127.0f : -1.0f,
                       marker == 1 ? CellStyle::Focused : CellStyle::Normal);
    widgets::ActionCell(g, Cell(0, 2), "PLAY", state.gridDown[2], kPlay);
    widgets::ActionCell(g, Cell(0, 3), "SAVE", state.gridDown[3], kTab);
    char length[24];
    std::snprintf(length, sizeof(length), "%.2fS", sampler.sampleRate > 0 ? static_cast<double>(sampler.frameCount) / sampler.sampleRate : 0.0);
    g.Text(panel.x, panel.y + 1, "TAKE", kDim, Font::Micro);
    g.Text(panel.x + panel.width - Gfx::Measure(length, Font::Micro), panel.y + 1, length, kDim, Font::Micro);
    const Rect area{panel.x, panel.y + 10, panel.width, panel.height - 10};
    Bars(g, area, sampler, hasStart ? start.value / 127.0f : 0.0f, hasEnd ? end.value / 127.0f : 1.0f, false);
    if (sampler.previewing && sampler.frameCount > 0)
    {
      const float cursor = static_cast<float>(sampler.previewCursorFrame) / static_cast<float>(sampler.frameCount);
      g.VLine(area.x + static_cast<int>(fx::Clamp01(cursor) * static_cast<float>(area.width - 1)), area.y, area.height, kPlay);
    }
    return;
  }

  const bool recording = sampler.recording;
  RecordKeyCell(g, Cell(0, 0), recording, state.gridDown[0]);
  const int gainDb = manager.RecordGainDb();
  char gain[16];
  std::snprintf(gain, sizeof(gain), "%s%d DB", gainDb > 0 ? "+" : "", gainDb);
  widgets::ParamCell(g, Cell(0, 1), "GAIN", gain, static_cast<float>(gainDb) / synth::SamplerManager::kMaxRecordGainDb, CellStyle::Focused);

  // The time against the limit: the card's, or what fits in RAM without one.
  double limit = host.QuerySystem().storageReady ? synth::SamplerManager::kMaxRecordSeconds : synth::SamplerManager::kMaxRamRecordSeconds;
  if (recording && sampler.sampleRate > 0) limit = static_cast<double>(sampler.recordMaxFrameCount) / sampler.sampleRate;
  const double seconds = recording && sampler.sampleRate > 0 ? static_cast<double>(sampler.frameCount) / sampler.sampleRate : 0.0;
  const Rect time = CellSpan(0, 2, 1, 2);
  TimeText(g, time.x + 3, time.y + 3, seconds, recording ? kInk : kFaint);
  char maxText[16];
  std::snprintf(maxText, sizeof(maxText), "%.3gS MAX", limit);
  g.Text(time.x + time.width - Gfx::Measure(maxText, Font::Micro) - 1, time.y + 2, maxText, kDim, Font::Micro);
  widgets::LevelBar(g, Rect{time.x + 3, time.y + time.height - 3, time.width - 4, 2}, recording ? sampler.recordProgress : 0.0f, kRec, kFaint);

  const Rect bars{panel.x, panel.y, panel.width - 10, panel.height};
  InputMeter(g, Rect{panel.x + panel.width - 5, panel.y + 1, 5, panel.height - 2}, recording ? RecentLevel(sampler) : 0.0f);
  if (recording)
  {
    Bars(g, bars, sampler, 0.0f, 1.0f, true);
    return;
  }
  g.DottedHLine(bars.x, bars.y + bars.height / 2, bars.width, kFaint, 3);
  const bool error = sampler.phase == synth::SamplerTrackPhase::Error;
  const std::string_view text = error ? std::string_view(sampler.statusText) : std::string_view("MIC READY");
  const int width = std::min(bars.width, Gfx::Measure(text, Font::Small) + 10);
  const Rect label{bars.x + (bars.width - width) / 2, bars.y + bars.height / 2 - 6, width, 13};
  g.Fill(label, kBlack);
  g.TextAligned(label, text, error ? kRec : kDim, Font::Small, Align::Center);
}

// RECORD (hold OK in SEQ): the record settings on row 0, the captured takes on rows 1 and 2 (newest
// first) and below them the bar of the chosen take that OK would put into a pattern.
void DrawRecord(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const RecordState& rec = state.record;
  const std::uint32_t now = animator.Now();
  const bool armed = controller.HostView().Sequencer().RecordArmed();
  const std::array<std::pair<const char*, std::string>, 3> settings{{
      {"REC", armed ? "ON" : "OFF"},
      {"SNAP", rec.snap ? "STEPS" : "FREE"},
      {"VIEW", controller.PageTab(Scene::Sequence, 0)},
  }};
  g.DottedHLine(Cell(0, 3).x, Cell(0, 3).y + Cell(0, 3).height - 1, Cell(0, 3).width, kShade);
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
      const auto& note = rec.TakeNote(*take, index);
      const int x = cell.x + 5 + static_cast<int>(static_cast<std::uint64_t>(note.startMs) * static_cast<std::uint64_t>(cell.width - 8) / span);
      g.Pixel(x, cell.y + cell.height - 3 - (note.note % 12) / 4, focused ? kBlack : color);
    }
  }
  // Row 3: what OK does now.
  const Rect area{Cell(3, 0).x, Cell(3, 0).y, Cell(3, 3).x + Cell(3, 3).width - Cell(3, 0).x, Cell(3, 0).height};
  const CaptureTake* take = rec.focus >= kRecordFirstTake ? controller.TakeAt(rec.focus - kRecordFirstTake) : nullptr;
  if (take == nullptr)
  {
    static constexpr std::array<const char*, 3> kHelp{{
        "REC: PLAY + PADS RECORD INTO THE PATTERN (ALSO FN + PLAY)",
        "SNAP: TAKES LAND ON WHOLE STEPS, OR KEEP THEIR TIMING",
        "VIEW: PIANO ROLL OR STEPS (TAP OK)",
    }};
    g.TextFit(area.x + 2, area.y + 4, area.width - 4, rec.focus < kHelp.size() ? kHelp[rec.focus] : "", kDim, Font::Micro);
    g.TextFit(area.x + 2, area.y + 15, area.width - 4,
              controller.TakeCount() == 0 ? "EVERYTHING THE PADS PLAY IS KEPT HERE AS TAKES" : "PICK A TAKE ABOVE TO PUT IT INTO A PATTERN",
              kFaint, Font::Micro);
    return;
  }
  const std::size_t bars = controller.TakeBars(*take);
  const std::size_t bar = std::min<std::size_t>(rec.takeBar, bars - 1);
  // No copy of the notes on the stack: a take can have 400 of them.
  struct Lane {
    int low = 127, high = 0;
    std::size_t count = 0;
  } range;
  controller.ForEachTakeBarNote(*take, bar, [](const Controller::PlacedNote& note, void* context) {
    auto& lane = *static_cast<Lane*>(context);
    lane.low = std::min<int>(lane.low, note.note);
    lane.high = std::max<int>(lane.high, note.note);
    ++lane.count;
  }, &range);
  const std::size_t count = range.count;
  const std::size_t steps = std::max<std::size_t>(1, controller.HostView().Sequencer().TrackStepCount(take->track));
  const int labelWidth = 44;
  char barLabel[24];
  std::snprintf(barLabel, sizeof(barLabel), "BAR %u/%u", static_cast<unsigned>(bar + 1), static_cast<unsigned>(bars));
  g.Text(area.x + 2, area.y + 3, barLabel, kInk);
  const std::uint32_t age = (now - take->endedAt) / 1000;
  g.Text(area.x + 2, area.y + 13, age < 60 ? std::to_string(age) + "S AGO" : std::to_string(age / 60) + "M AGO", kFaint, Font::Micro);
  const Rect lane{area.x + labelWidth, area.y + 2, area.width - labelWidth - 2, area.height - 4};
  g.Stroke(lane, kShade);
  struct Drawing {
    Gfx& g;
    Rect lane;
    int low;
    int span;
    std::size_t steps;
    Color color;
  } drawing{g, lane, range.low, std::max(1, range.high - range.low), steps, TrackColor(take->track)};
  controller.ForEachTakeBarNote(*take, bar, [](const Controller::PlacedNote& note, void* context) {
    auto& target = *static_cast<Drawing*>(context);
    const Rect& area = target.lane;
    const std::size_t pulses = target.steps * synth::kSequencerPulsesPerStep;
    const int x = area.x + 1 + static_cast<int>((note.step * synth::kSequencerPulsesPerStep + note.microstep * synth::kSequencerPulsesPerMicrostep) * (area.width - 2) / pulses);
    const int width = std::max(2, static_cast<int>(note.lengthPulses * (area.width - 2) / pulses));
    const int y = area.y + area.height - 3 - (note.note - target.low) * (area.height - 4) / target.span;
    target.g.Fill(Rect{x, y, std::min(width, area.x + area.width - 1 - x), 2}, target.color);
  }, &drawing);
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
    case ModalKind::EnginePicker: DrawEnginePicker(g, animator, modal); break;
    case ModalKind::SampleRecord: DrawSampleRecord(g, controller); break;
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
    case ModalKind::TextEntry:
      switch (modal.textPurpose)
      {
        case TextPurpose::ProjectSaveAs: return "ENTER SAVE PROJECT  ESC CANCEL";
        case TextPurpose::PresetSaveAs: return "ENTER SAVE PRESET  ESC CANCEL";
        case TextPurpose::SampleRename: return "ENTER RENAME  ESC CANCEL";
        case TextPurpose::ProjectRename: return "ENTER RENAME PROJECT  ESC CANCEL";
        case TextPurpose::Search: return "ENTER SEARCH  ESC CANCEL";
      }
      return "";
    case ModalKind::Confirm:
      if (modal.confirmPurpose == ConfirmPurpose::Notice) return "ENTER OR ESC CLOSES";
      return modal.danger ? "ENTER CONFIRM   ESC KEEP" : "ENTER YES   ESC NO";
    case ModalKind::PresetBrowser:
      return modal.search.empty() ? "ENTER LOAD PRESET  ESC BACK" : "ENTER LOAD  DEL ERASE  ESC CLEAR";
    case ModalKind::SampleBrowser: return "ENTER LOAD  FN+F SEARCH  FN+ENTER EDIT";
    case ModalKind::SampleOptions: return "ENTER CHOOSE   ESC BACK";
    case ModalKind::EnginePicker: return "GRID OR SUB ADD   ENTER SWITCH";
    case ModalKind::Menu: return "GRID RUNS   FN OR ESC CLOSES";
    case ModalKind::Record: return "+- BAR  OK NEW PATTERN  FN+OK MERGE";
    case ModalKind::SampleRecord:
    {
      const auto sampler = controller.HostView().Sampler().BuildUiState(controller.State().track);
      if (sampler.recording) return "ENTER STOP   ESC CANCEL";
      if (sampler.hasDraft) return "ENTER START/END   ESC DISCARD";
      return "ENTER RECORD   ESC CLOSE";
    }
    case ModalKind::None:
    default: return "";
  }
}

}  // namespace groove::render
