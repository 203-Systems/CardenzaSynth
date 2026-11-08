#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "CardenzaBuildInfo.h"
#include "Applications/Synth/UI/Groove/Render/Effects.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"
#include "Applications/Synth/UI/Groove/Render/Widgets.h"

#include "Library/Synth/Core/Engines/Chop/ChopSynthEngine.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

namespace groove::render {

namespace {

using namespace groove::theme;
using widgets::Cell;
using widgets::CellSpan;
using widgets::TrackCell;
using widgets::CellStyle;

constexpr std::uint32_t kRippleMs = 300;
// The playhead flashes the step it enters, then the step stays outlined while it plays.
constexpr std::uint32_t kStepFlashMs = 160;
constexpr std::uint32_t kAfterglowMs = 380;

float ParamFraction(const synth::SynthParameterInfo& info)
{
  if (info.kind != synth::SynthParameterKind::Continuous || info.maxValue <= info.minValue) return -1.0f;
  return static_cast<float>(info.value - info.minValue) / static_cast<float>(info.maxValue - info.minValue);
}

float EngineValue(const synth::ISynthEngine* engine, const char* id, float fallback = 0.0f)
{
  synth::SynthParameterInfo info;
  if (engine == nullptr || !engine->GetParameterInfo(id, info) || info.maxValue <= info.minValue) return fallback;
  return static_cast<float>(info.value - info.minValue) / static_cast<float>(info.maxValue - info.minValue);
}

int EngineRaw(const synth::ISynthEngine* engine, const char* id, int fallback = 0)
{
  synth::SynthParameterInfo info;
  return engine != nullptr && engine->GetParameterInfo(id, info) ? info.value : fallback;
}

// Engine parameter that glides to its new value, so graphs morph instead of jumping.
float Glide(Animator& animator, std::size_t track, const synth::ISynthEngine* engine, const char* id, float fallback = 0.0f)
{
  return animator.Smooth(Animator::Key(id, static_cast<std::uint32_t>(track) * 977u + 13u), EngineValue(engine, id, fallback), 80.0f);
}

bool InArea(const GraphicArea& area, std::size_t index)
{
  const std::size_t row = index / kGridColumns;
  const std::size_t column = index % kGridColumns;
  return area.rows > 0 && row >= area.row && row < area.row + area.rows && column >= area.column && column < area.column + area.columns;
}

std::string Pad2(std::size_t value)
{
  char text[12];
  std::snprintf(text, sizeof(text), "%02u", static_cast<unsigned>(value));
  return text;
}

void GridRipple(Gfx& g, Animator& animator, std::size_t index, Color color)
{
  const std::uint32_t pressed = animator.GridPressedAt(index);
  if (pressed == 0) return;
  fx::Ripple(g, Cell(index / kGridColumns, index % kGridColumns), fx::Progress(animator.Now(), pressed, kRippleMs), color);
}

// How hard, in the four steps of the pad rows: solid, checker, a quarter, an eighth.
constexpr std::array<int, 4> kDrumDensity{{16, 8, 4, 2}};
int DrumDensity(std::uint8_t velocity) { return kDrumDensity[DrumRowForVelocity(velocity)]; }

// The kit's pads from the top row down: the pad order read backwards, so pad 1 (BD) is the bottom
// row. The matrix then reads like the piano roll -- low at the bottom -- without moving any key:
// pad 1 is still BD, and the columns of keys are untouched.
//
// The note scope walks by MIDI pitch, which is NOT this order (CP is 39, below CH's 42), so the
// walk and the rows disagree on CP / OH / TM. That is a known trade: the screen follows the hands,
// the scope follows the notes.
constexpr std::array<std::size_t, 8> kDrumRowPad{{7, 6, 5, 4, 3, 2, 1, 0}};


// Velocity is height now that ROLL cuts the bar: as tall as it is loud, standing on the bottom of
// the space it has, never shorter than two pixels (a hit too quiet to see is a hit you cannot edit).
// A density would fight the notches; a height reads through them.
Rect VelocityBar(const Rect& space, std::uint8_t velocity)
{
  if (space.height <= 3) return space;
  const int wanted = (space.height * static_cast<int>(velocity) + 63) / 127;
  const int height = std::clamp(wanted, 3, space.height);
  return Rect{space.x, space.y + space.height - height, space.width, height};
}

// ROLL in a GRID square: a 2 x 2 corner of dots filled clockwise — the top row for x2, then the
// right one of the second row for x3, then all four for x4. Off shows nothing. The square keeps its
// full body for the hit itself, so the count never eats the step's own shape.
void DrawRollDots(Gfx& g, const Rect& cell, std::uint8_t roll)
{
  if (roll <= 1) return;
  constexpr int kDot = 2;
  constexpr int kStep = 3;  // dot + one pixel of gap
  const int left = cell.x + cell.width - 2 - (2 * kStep - 1);
  const int top = cell.y + 2;
  g.Fill(Rect{left, top, kDot, kDot}, kBlack);
  g.Fill(Rect{left + kStep, top, kDot, kDot}, kBlack);
  if (roll >= 3) g.Fill(Rect{left + kStep, top + kStep, kDot, kDot}, kBlack);
  if (roll >= 4) g.Fill(Rect{left, top + kStep, kDot, kDot}, kBlack);
}

// ROLL: the bar is cut into the sub-hits it retriggers on, one notch per repeat, so a sixteenth
// rolled four times reads as four 64ths.
void DrawRollNotches(Gfx& g, const Rect& bar, int roll)
{
  if (roll <= 1 || bar.width < 3) return;
  int last = -1;
  for (int hit = 1; hit < roll; ++hit)
  {
    // Clamped to the inside of the bar, and never twice on one pixel: a four-pulse step is four
    // pixels wide, so its four sub-hits can only read as a denser pattern there.
    const int x = bar.x + (bar.width * hit) / roll;
    if (x <= bar.x || x >= bar.x + bar.width - 1 || x == last) continue;
    g.Fill(Rect{x, bar.y, 1, bar.height}, kBlack);
    last = x;
  }
}

// How tall a bar stands in a room this high: as loud as it is tall, two pixels at the least so a soft
// hit is still a hit. A lane of six pixels reads as three heights, a lane of twelve as ten - the bar
// fills whatever room the screen gives it, and a small sample's fat rows do not hold a thin note.
int BarHeight(int room, std::uint8_t velocity)
{
  if (room <= 0) return 0;
  return std::clamp((room * static_cast<int>(velocity) + 63) / 127, std::min(2, room), room);
}

// The GRID beside a step matrix: four squares, a name line above them. The matrix takes its rows from
// the same band, and so does the lane roll, so the two screens show the same slice at the same height.
constexpr int kStepGap = 2;
constexpr int kStepNameHeight = 10;
constexpr int kStepSquare = (kContentHeight - kStepNameHeight - 3 * kStepGap) / 4;
constexpr int kStepBand = kStepSquare * 4 + 3 * kStepGap;

// A chop's hit on the step matrix: the cell's whole width (TIME stays the GRID's business) and as
// tall as the hit is hard, standing on the cell's floor.
Rect ChopBar(const Rect& cell, std::uint8_t velocity)
{
  const int height = BarHeight(cell.height, velocity);
  return Rect{cell.x, cell.y + cell.height - height, cell.width, height};
}

// ROLL on the matrix: the bar cut into the repeats the slice fires on, the way the GRID square cuts
// its own - two, three or four parts.
void DrawChopCuts(Gfx& g, const Rect& bar, std::uint8_t roll)
{
  for (int part = 1; part < roll; ++part)
  {
    const int x = bar.x + bar.width * part / roll;
    if (x <= bar.x || x >= bar.x + bar.width) continue;
    g.Fill(Rect{x, bar.y, 1, bar.height}, kBlack);
  }
}

// The kit's eight drums in one row, like the eight pad columns on the keyboard (a column is a drum).
// With fill (VOICE) they take the whole area, as the keyboard of a melodic track does, less the room
// a hit kicks them up; otherwise (the KIT page's graphic) a strip of at most 24 px in its middle.
void DrawPads(Gfx& g, const Controller& controller, Animator& animator, const Rect& area, const synth::ISynthEngine* engine,
              bool fill = false)
{
  constexpr int kGap = 2;
  constexpr int kKickRoom = 3;  // how far a hit kicks a pad up, at most
  const int padWidth = (area.width - kGap * 7) / 8;
  const int padHeight = fill ? area.height - kKickRoom : std::min(area.height, 24);
  const int left = area.x + (area.width - (padWidth * 8 + kGap * 7)) / 2;
  const int top = fill ? area.y + kKickRoom : area.y + (area.height - padHeight) / 2;
  static constexpr std::array<const char*, 8> kIds{{"kick.level", "snare.level", "closed_hat.level", "open_hat.level",
                                                    "clap.level", "tom.level", "cowbell.level", "maraca.level"}};
  const std::uint32_t now = animator.Now();
  for (std::size_t pad = 0; pad < 8; ++pad)
  {
    const Rect rect{left + static_cast<int>(pad) * (padWidth + kGap), top, padWidth, padHeight};
    const bool held = controller.IsNoteHeld(kDrumNotes[pad]);
    const float hit = fx::Decay(now, animator.NoteHitAt(kDrumNotes[pad]), kAfterglowMs);
    const float level = Glide(animator, controller.State().track, engine, kIds[pad], 0.8f);
    // How hard shows in how dense the pad fills, and in how far a hit kicks it up.
    const float strength = static_cast<float>(animator.DrumHitVelocity(pad)) / 127.0f;
    const int density = DrumDensity(animator.DrumHitVelocity(pad));
    const int kick = static_cast<int>(hit * hit * strength * 3.0f);
    const Rect body{rect.x, rect.y - kick, rect.width, rect.height};
    const int filled = static_cast<int>(level * static_cast<float>(body.height - 2));
    g.Dither(Rect{body.x + 1, body.y + body.height - 1 - filled, body.width - 2, filled}, kFaint, 4);
    if (held) g.Dither(body, kInk, density);
    else if (hit > 0.0f) g.Dither(body, kInk, static_cast<int>(hit * static_cast<float>(density)));
    g.Stroke(body, held || hit > 0.0f ? kInk : kFaint);
    // The name stays readable on any density.
    const int nameWidth = Gfx::Measure(kDrumNames[pad]);
    const int nameX = body.x + (body.width - nameWidth) / 2;
    if (held || hit > 0.0f) g.Fill(Rect{nameX - 1, body.y + 2, nameWidth + 2, 9}, kBlack);
    g.Text(nameX, body.y + 3, kDrumNames[pad], kInk);
    if (hit > 0.0f) fx::Ripple(g, body, 1.0f - hit, kInk);
  }
}

void DrawMiniPiano(Gfx& g, const Rect& rect, const Controller& controller, Animator& animator, int baseNote, bool showScale = false,
                   const std::array<bool, 128>* marked = nullptr)
{
  // The pads' two octaves: from baseNote (a C) to the C two octaves up. Outlined white keys, dithered
  // black keys; held notes (whole chords) light up in the track color and fade after release. With
  // showScale, scale tones carry a dot and the root a bar.
  static constexpr std::array<int, 7> kWhite{{0, 2, 4, 5, 7, 9, 11}};
  static constexpr std::array<int, 7> kBlackAfter{{1, 3, -1, 6, 8, 10, -1}};
  const std::uint32_t now = animator.Now();
  const KeyboardSettings& keys = controller.State().keyboard;
  const Color held = TrackColor(controller.State().track);
  const int whiteCount = 15;  // C to C, two octaves
  const int keyWidth = rect.width / whiteCount;
  const int left = rect.x + (rect.width - keyWidth * whiteCount) / 2;
  auto mark = [&](const Rect& key, int note, Color color) {
    if (!showScale || !InScale(keys, note)) return;
    const bool root = ((note - keys.root) % 12 + 12) % 12 == 0;
    if (root) g.Fill(Rect{key.x + 1, key.y + key.height - 3, key.width - 2, 2}, color);
    else g.Pixel(key.x + key.width / 2, key.y + key.height - 3, color);
  };
  for (int index = 0; index < whiteCount; ++index)
  {
    const int note = baseNote + (index / 7) * 12 + kWhite[static_cast<std::size_t>(index % 7)];
    const Rect key{left + index * keyWidth, rect.y, keyWidth - 1, rect.height};
    const float glow = fx::Decay(now, animator.NoteHitAt(note), kAfterglowMs);
    const bool down = controller.IsChordNoteHeld(note);
    if (down) g.Fill(key, held);
    else
    {
      if (marked != nullptr && note >= 0 && note < 128 && (*marked)[static_cast<std::size_t>(note)]) g.Dither(key, held, 8);
      if (glow > 0.0f) g.Dither(key, kInk, static_cast<int>(glow * 11.0f));
      g.Stroke(key, kDim);
    }
    mark(key, note, down ? kBlack : kInk);
  }
  for (int index = 0; index < whiteCount; ++index)
  {
    const int black = kBlackAfter[static_cast<std::size_t>(index % 7)];
    if (black < 0 || index + 1 == whiteCount) continue;
    const int note = baseNote + (index / 7) * 12 + black;
    const Rect key{left + index * keyWidth + keyWidth * 2 / 3, rect.y, keyWidth * 2 / 3, rect.height * 3 / 5};
    const bool down = controller.IsChordNoteHeld(note);
    const float glow = fx::Decay(now, animator.NoteHitAt(note), kAfterglowMs);
    const bool chordTone = marked != nullptr && note >= 0 && note < 128 && (*marked)[static_cast<std::size_t>(note)];
    g.Fill(key, down || chordTone ? held : kBlack);
    if (!down && !chordTone)
    {
      g.Dither(Rect{key.x + 1, key.y, key.width - 2, key.height - 1}, glow > 0.0f ? held : kDim, glow > 0.0f ? 4 + static_cast<int>(glow * 10.0f) : 8);
      g.Stroke(key, kFaint);
    }
    mark(key, note, kInk);
  }
  // Each octave's C named, as on the pads' bottom left of each pair of rows.
  for (const int octave : {0, 1})
  {
    const int c = baseNote + octave * 12;
    g.Text(left + octave * 7 * keyWidth + 2, rect.y + rect.height - 9, NoteName(c), controller.IsChordNoteHeld(c) ? kBlack : kDim);
  }
}

// The part of the file the chop's waveform shows: all of it, or a magnified window centred on the
// picked slice's start. `span` is 1 / zoom and `start` is where the window opens.
struct ChopView {
  float start = 0.0f;
  float span = 1.0f;
};

// The part of the file the SETTING page keeps. Every page that shows a chop's waveform shows this and
// nothing else: whatever the trim cut away cannot be played, so it is not drawn either.
ChopView ChopTrimWindow(const synth::ISynthEngine* engine)
{
  synth::SynthParameterInfo trimIn;
  synth::SynthParameterInfo trimOut;
  if (engine == nullptr || !engine->GetParameterInfo(synth::kChopSampleStartId, trimIn) ||
      !engine->GetParameterInfo(synth::kChopSampleEndId, trimOut))
  {
    return {};
  }
  const float low = static_cast<float>(trimIn.value) / static_cast<float>(synth::kChopEndOfFile);
  const float high = static_cast<float>(trimOut.value) / static_cast<float>(synth::kChopEndOfFile);
  return {low, (std::max)(0.002f, high - low)};
}

ChopView ChopWindow(const Controller& controller, const synth::ISynthEngine* engine)
{
  const int zoom = controller.ChopZoomFactor();
  // The SAMPLE page shows the trim: at 1x the window is that trim, and at 2x or 4x it is a slice of
  // it, centred on the point the page is editing - the picked slice's start, or its end while that
  // cell has the focus.
  const ChopView trim = ChopTrimWindow(engine);
  if (engine == nullptr) return {};
  if (zoom <= 1) return trim;
  const float low = trim.start;
  const float high = trim.start + trim.span;
  const float whole = trim.span;
  const std::size_t picked = controller.ChopSlice();
  const std::string id = controller.ChopEndFocused() ? synth::ChopEndParameterId(picked)
                                                     : synth::ChopStartParameterId(picked);
  synth::SynthParameterInfo point;
  if (!engine->GetParameterInfo(id, point)) return trim;
  const float span = whole / static_cast<float>(zoom);
  const float center = static_cast<float>(point.value) / static_cast<float>(synth::kChopEndOfFile);
  // Pinned against the trim rather than left hanging over black: at the first slice the window opens
  // at the trim's start, and its mark slides off centre instead of off screen.
  const float maxStart = (std::max)(low, high - span);
  return {std::clamp(center - span * 0.5f, low, maxStart), span};
}

// One of a chop's two kinds of point: a start carries its flag on the top edge, sticking out to the
// right; an end has it on the bottom edge, sticking out to the left. The start's line stops two pixels
// short of the bottom, so the pair reads apart even when they share a column.
void DrawPointMark(Gfx& g, const Rect& graph, int x, Color color, bool end)
{
  if (end)
  {
    g.VLine(x, graph.y + 2, graph.height - 2, color);
    g.Fill(Rect{x - 2, graph.y + graph.height - 3, 3, 3}, color);
    return;
  }
  g.VLine(x, graph.y + 2, graph.height - 4, color);
  g.Fill(Rect{x, graph.y, 3, 3}, color);
}

// The SETTING page's marks: the trim the slices live inside, over the whole file, and where the
// playhead is in it.
void DrawChopTrim(Gfx& g, const synth::ISynthEngine* engine, std::size_t track, const Rect& graph)
{
  const Color color = TrackColor(track);
  for (const bool end : {false, true})
  {
    synth::SynthParameterInfo point;
    if (!engine->GetParameterInfo(end ? synth::kChopSampleEndId : synth::kChopSampleStartId, point)) continue;
    const int x = graph.x + (graph.width - 1) * point.value / synth::kChopEndOfFile;
    DrawPointMark(g, graph, x, color, end);
  }
  const auto head = synth::audio::ChopPlayhead(track);
  if (head.slice < 0) return;
  const int x = graph.x + static_cast<int>(static_cast<float>(graph.width - 1) * std::clamp(head.position, 0.0f, 1.0f));
  g.Fill(Rect{x, graph.y, 2, graph.height}, color);
}

// What a chop draws over its waveform: the slice starts (on the page that edits them) and the
// playhead, in the track's colour so it reads as this track's sound rather than as a scrub bar.
void DrawChopWave(Gfx& g, const Controller& controller, const synth::ISynthEngine* engine, std::size_t track,
                  const Rect& graph, std::uint32_t now, bool markers, ChopView view = ChopView{})
{
  const std::size_t slices = controller.ChopSliceCount();
  const Color trackColor = TrackColor(track);
  // A position in the file to a column of the window, and whether it is inside it at all.
  const auto columnFor = [&](float fraction, int& x) {
    if (fraction < view.start || fraction > view.start + view.span) return false;
    x = graph.x + static_cast<int>((fraction - view.start) / view.span * static_cast<float>(graph.width - 1));
    return true;
  };

  // Every slice draws two marks in the track's colour: its start as a line down the waveform with a
  // small flag on top, its end as the same line with the mark hanging at the bottom and a tail
  // curling out of it - the apostrophe the slice page's guide uses, so the two read apart even when
  // they sit on the same column. The picked slice's pair alternates with white at 2 Hz, the way a
  // scoped note blinks.
  if (markers)
  {
    const std::size_t picked = controller.ChopSlice();
    const bool blinkWhite = ((now / 250) & 1) != 0;
    const auto drawSlice = [&](std::size_t slice, Color color) {
      // A slice's start: the line, and a flag on its top edge sticking out to the right.
      synth::SynthParameterInfo start;
      if (engine->GetParameterInfo(synth::ChopStartParameterId(slice), start))
      {
        int x = 0;
        if (columnFor(static_cast<float>(start.value) / static_cast<float>(synth::kChopEndOfFile), x))
          DrawPointMark(g, graph, x, color, false);
      }
      // Its end: the same line, with the flag at the bottom sticking out to the left, so the two read
      // apart even when they share a column.
      synth::SynthParameterInfo end;
      if (!engine->GetParameterInfo(synth::ChopEndParameterId(slice), end)) return;
      int x = 0;
      if (!columnFor(static_cast<float>(end.value) / static_cast<float>(synth::kChopEndOfFile), x)) return;
      DrawPointMark(g, graph, x, color, true);
    };
    // The picked slice is drawn last, over the other fifteen: its two flags are the ones the page is
    // moving, and thirty-two lines on a 240 pixel screen would otherwise swallow them.
    for (const bool front : {false, true})
      for (std::size_t slice = 0; slice < slices; ++slice)
        if ((slice == picked) == front) drawSlice(slice, front && blinkWhite ? kInk : trackColor);
  }

  // The playhead: one line, only while a slice is sounding, where it really is in the file. Taking the
  // position from the audio means a slice previewed from its last second starts there, not at the top.
  const auto head = synth::audio::ChopPlayhead(track);
  if (head.slice < 0 || static_cast<std::size_t>(head.slice) >= slices) return;
  int x = 0;
  if (!columnFor(head.position, x)) return;
  g.Fill(Rect{x, graph.y, 2, graph.height}, trackColor);
}

void DrawVoice(Gfx& g, const Controller& controller, Animator& animator, const Rect& area, bool chopFile = false)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto* engine = host.Engine(state.track);
  const std::string name = engine != nullptr ? engine->CurrentPresetName() : std::string("NO ENGINE");
  const Font font = Gfx::Measure(name, Font::Huge) <= area.width ? Font::Huge : Font::Large;
  const bool huge = font == Font::Huge;
  // The preset name decodes whenever it changes (SUB / ADD, browser, track switch). A name in the
  // huge font sits higher and closer to the info line, so the keyboard or the drum pads below keep
  // one height whatever the name.
  const std::uint32_t changed = std::max(animator.PresetChangedAt(state.track), animator.TrackChangedAt());
  fx::DecodeText(g, area.x, area.y + (huge ? -1 : 2), name, kInk, font, fx::Progress(now, changed, 420), changed / 7 + now / 60, area.width);
  const int infoY = area.y + (huge ? 23 : 21);
  int x = g.Text(area.x, infoY, EngineShortName(host.EngineKind(state.track)), TrackColor(state.track));
  if (engine != nullptr && engine->CurrentPresetId().rfind("user.", 0) == 0) x = g.Text(x + 6, infoY, "USER", kFn);
  if (chopFile)
  {
    // A chop has no keyboard: the file itself sits where the keys would be, at the height the SAMPLE
    // page gives it, so both pages read as the same picture.
    const auto sampler = host.Sampler().BuildUiState(state.track);
    std::string detail = sampler.hasSample ? sampler.sampleName : std::string("NO SAMPLE - FILE OR REC");
    if (sampler.hasSample && sampler.sampleRate > 0)
    {
      char seconds[16];
      std::snprintf(seconds, sizeof(seconds), "  %.2fS",
                    static_cast<double>(sampler.frameCount) / static_cast<double>(sampler.sampleRate));
      detail += seconds;
    }
    g.Text(x + 8, infoY, detail, sampler.hasSample ? kDim : kFaint);

    // The SAMPLE page's graph rect, unchanged: same place, same height, so the two pages read as the
    // same picture with one line of text above it.
    const Rect sampleArea = CellSpan(2, 0, 2, 4);
    const Rect graph{sampleArea.x + 2, sampleArea.y + 10, sampleArea.width - 4, sampleArea.height - 10};
    // The trimmed part of the file, edge to edge - the same picture the SAMPLE page draws, without
    // the marks and without the magnifier.
    const ChopView view = ChopTrimWindow(engine);
    if (view.span < 0.9995f || view.start > 0.0005f)
      widgets::WaveformWindow(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                              view.start, view.start + view.span, 0.0f, false, 0.5f);
    else
      widgets::Waveform(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                        0.0f, 1.0f, 0.0f, false, 0.5f);
    if (engine != nullptr) DrawChopWave(g, controller, engine, state.track, graph, now, false, view);
    return;
  }
  if (host.EngineKind(state.track) != synth::SynthEngineKind::Drums)
  {
    const KeyboardSettings& keys = state.keyboard;
    std::string detail = std::string(kLayoutNames[static_cast<std::size_t>(keys.layout)]);
    if (keys.layout != KeyboardLayout::Piano) detail += std::string(" ") + kNoteNames[static_cast<std::size_t>(keys.root)];
    if (keys.layout == KeyboardLayout::Scale) detail += std::string(" ") + kScales[keys.scale].label;
    detail += "  OCT " + std::string(keys.octave > 0 ? "+" : "") + std::to_string(keys.octave);
    g.Text(x + 8, infoY, detail, kDim);
  }
  const int pianoTop = area.y + 33;
  const Rect lower{area.x, pianoTop, area.width, area.y + area.height - pianoTop};
  if (host.EngineKind(state.track) == synth::SynthEngineKind::Drums)
  {
    DrawPads(g, controller, animator, lower, engine, true);
    return;
  }
  const int base = synth::kHomePerformanceBaseMidiNote + state.keyboard.octave * 12;
  DrawMiniPiano(g, lower, controller, animator, base);
}

// The track's insert chain left to right (drive, crush, filter, chorus) and its two sends dropping
// to the master delay and reverb. Active stages fill with their amount; bypassed ones are dotted.
// While the track sounds, signal dots travel the line and into every send that is open.
void DrawFxChain(Gfx& g, const Controller& controller, Animator& animator, const Rect& area)
{
  const Host& host = controller.HostView();
  const std::size_t track = controller.State().track;
  const auto* engine = host.Engine(track);
  const std::uint32_t now = animator.Now();
  const float activity = host.TrackActivity(track);
  const int lineY = area.y + 12;
  const int startX = area.x + 14;
  const int endX = area.x + area.width - 22;
  g.Text(area.x, lineY - 3, "IN", kDim);
  g.HLine(startX, lineY, endX - startX, kFaint);
  g.Text(endX + 3, lineY - 3, "OUT", kDim);

  struct Stage { const char* tag; const char* id; float amount; };
  const int filterValue = EngineRaw(engine, "fx.filter", synth::fx::kFilterCenter);
  const std::array<Stage, 4> stages{{
      {"DRV", "fx.drive", Glide(animator, track, engine, "fx.drive")},
      {"CRS", "fx.crush", Glide(animator, track, engine, "fx.crush")},
      {"FLT", "fx.filter", std::fabs(static_cast<float>(filterValue - synth::fx::kFilterCenter)) / 64.0f},
      {"CHO", "fx.chorus", static_cast<float>(EngineRaw(engine, "fx.chorus")) / 3.0f},
  }};
  const int pitch = (endX - startX) / 4;
  const int boxWidth = pitch - 8;
  for (std::size_t index = 0; index < stages.size(); ++index)
  {
    const Stage& stage = stages[index];
    const Rect box{startX + 4 + static_cast<int>(index) * pitch, lineY - 9, boxWidth, 18};
    const bool on = stage.amount > 0.001f;
    g.Fill(box, kBlack);
    if (on)
    {
      g.Dither(box, kInk, 2 + static_cast<int>(stage.amount * 10.0f));
      g.Stroke(box, kInk);
    }
    else
    {
      g.DottedHLine(box.x, box.y, box.width, kFaint);
      g.DottedHLine(box.x, box.y + box.height - 1, box.width, kFaint);
      g.DottedVLine(box.x, box.y, box.height, kFaint);
      g.DottedVLine(box.x + box.width - 1, box.y, box.height, kFaint);
    }
    const Rect label{box.x + 1, box.y + 1, box.width - 2, 8};
    if (on) g.Fill(Rect{label.x, label.y, Gfx::Measure(stage.tag) + 4, 9}, kBlack);
    g.Text(label.x + 2, label.y + 1, stage.tag, on ? kInk : kFaint);
    if (index == 2 && on)
    {
      // Filter: a tiny slope, falling for low-pass, rising for high-pass.
      const bool highPass = filterValue > synth::fx::kFilterCenter;
      const int x0 = box.x + box.width - 14;
      for (int dx = 0; dx < 11; ++dx)
      {
        const int height = highPass ? std::min(6, dx) : std::min(6, 10 - dx);
        g.Pixel(x0 + dx, box.y + box.height - 3 - height, kInk);
      }
    }
    if (index == 3 && on)
    {
      const std::string mode = synth::fx::kChorusModes[static_cast<std::size_t>(std::clamp(EngineRaw(engine, "fx.chorus"), 0, 3))];
      g.Text(box.x + box.width - Gfx::Measure(mode) - 2, box.y + 10, mode, kInk);
    }
  }

  // Sends: post-fader taps into the master buses.
  const int tapX = endX - 6;
  const int busY = area.y + 30;
  const std::array<const char*, 2> sendIds{{"fx.delay", "fx.reverb"}};
  const std::array<const char*, 2> sendTags{{"DELAY", "REVERB"}};
  const std::array<std::size_t, 2> buses{{synth::fx::kBusDelay, synth::fx::kBusReverb}};
  const int sendWidth = 70;
  bool anySend = false;
  for (std::size_t send = 0; send < 2; ++send)
  {
    const float amount = Glide(animator, track, engine, sendIds[send]);
    anySend = anySend || amount > 0.001f;
    const Rect box{area.x + 52 + static_cast<int>(send) * (sendWidth + 14), busY + 4, sendWidth, area.height - (busY - area.y) - 5};
    const int dropX = box.x + box.width / 2;
    if (amount > 0.001f)
    {
      g.HLine(std::min(dropX, tapX), busY, std::abs(tapX - dropX) + 1, kDim);
      g.VLine(dropX, busY, 4, kDim);
    }
    g.Stroke(box, amount > 0.001f ? kDim : kShade);
    g.Text(box.x + 3, box.y + 3, sendTags[send], amount > 0.001f ? kInk : kFaint);
    widgets::LevelBar(g, Rect{box.x + 3, box.y + box.height - 5, box.width - 6, 2}, amount, kInk, kFaint);
    // The return meter of the master bus: how much is coming back from it right now.
    const float wet = host.FxReturnLevel(buses[send]);
    if (!host.FxBusAvailable(buses[send])) g.Text(box.x + box.width - Gfx::Measure("NO MEM") - 3, box.y + 3, "NO MEM", kRec);
    else if (wet > 0.01f) g.Dither(Rect{box.x + box.width - 8, box.y + 2, 5, box.height - 9}, kTab, static_cast<int>(4.0f + wet * 12.0f));
  }
  if (anySend) g.VLine(tapX, lineY, busY - lineY + 1, kDim);

  // Signal dots while the track sounds.
  if (activity > 0.02f)
  {
    const float travel = static_cast<float>(now % 900) / 900.0f;
    for (int dot = 0; dot < 3; ++dot)
    {
      const float position = std::fmod(travel + static_cast<float>(dot) / 3.0f, 1.0f);
      const int x = startX + static_cast<int>(position * static_cast<float>(endX - startX));
      g.Fill(Rect{x - 1, lineY - 1, 3, 3}, kTab);
    }
  }
}

void DrawSystemInfo(Gfx& g, const Controller& controller, Animator& animator, const Rect& panel);

void DrawGraphic(Gfx& g, const Controller& controller, Animator& animator, const InstrumentPage& page)
{
  const Host& host = controller.HostView();
  const UiState& state = controller.State();
  const std::size_t track = state.track;
  const std::uint32_t now = animator.Now();
  const auto* engine = host.Engine(track);
  const Rect area = CellSpan(page.area.row, page.area.column, page.area.rows, page.area.columns);
  const float activity = host.TrackActivity(track);
  switch (page.graphic)
  {
    case PageGraphic::Voice:
      DrawVoice(g, controller, animator, area);
      break;
    case PageGraphic::ChopFile:
      DrawVoice(g, controller, animator, area, true);
      break;
    case PageGraphic::Algorithm:
      widgets::Dx7Algorithm(g, Rect{area.x + 4, area.y, area.width - 8, area.height}, EngineRaw(engine, "global.algorithm", 1), 0,
                            EngineRaw(engine, "global.feedback"), activity, now);
      break;
    case PageGraphic::OperatorEnvelope:
    {
      // DX7 envelope: R1..R3 climb to L1..L3, L3 holds while the key is down, R4 returns to L4.
      // Higher rates are shorter segments; the graph is scaled by the operator output.
      const std::string prefix = "op" + std::to_string(page.op) + ".";
      auto value = [&](const char* id, float fallback = 0.0f) { return Glide(animator, track, engine, (prefix + id).c_str(), fallback); };
      const float output = 0.25f + 0.75f * value("output", 1.0f);
      const std::array<float, 4> levels{{value("level1", 1.0f), value("level2", 0.9f), value("sustain"), value("level4")}};
      const std::array<float, 4> rates{{value("attack"), value("decay"), value("rate3", 0.5f), value("release")}};
      float previous = levels[3];
      std::array<float, 5> times{};
      for (std::size_t segment = 0; segment < 3; ++segment)
      {
        times[segment] = (0.06f + (1.0f - rates[segment])) * (0.15f + std::fabs(levels[segment] - previous));
        previous = levels[segment];
      }
      times[3] = 0.35f;
      times[4] = (0.06f + (1.0f - rates[3])) * (0.15f + std::fabs(levels[2] - levels[3]));
      const bool carrier = widgets::Dx7IsCarrier(EngineRaw(engine, "global.algorithm", 1), page.op);
      const Rect graph{area.x + 2, area.y + 2, area.width - 30, area.height - 5};
      widgets::Envelope(g, graph, times, {{levels[0] * output, levels[1] * output, levels[2] * output, levels[2] * output, levels[3] * output}},
                        carrier ? kInk : kDim);
      // Operator tag: number, and whether it is heard (carrier) or modulates another operator.
      const Rect tag{area.x + area.width - 26, area.y + 2, 26, area.height - 4};
      g.Text(tag.x + 2, tag.y + 1, "OP" + std::to_string(page.op), kInk);
      g.Text(tag.x + 2, tag.y + 11, carrier ? "CAR" : "MOD", carrier ? kPlay : kDim);
      if (carrier && activity > 0.02f) g.Dither(Rect{tag.x, tag.y + 19, 22, 2}, kPlay, static_cast<int>(4.0f + activity * 12.0f));
      break;
    }
    case PageGraphic::OperatorLevels:
    {
      // Six operator outputs: carriers (heard) solid, modulators (timbre) dithered.
      const int algorithm = EngineRaw(engine, "global.algorithm", 1);
      g.Text(area.x + 2, area.y, "ALG " + std::to_string(algorithm), kDim);
      g.Text(area.x + area.width - Gfx::Measure("SOLID=HEARD") - 2, area.y, "SOLID=HEARD", kFaint);
      const int barWidth = (area.width - 4 - 5 * 6) / 6;
      const Rect bars{area.x + 2, area.y + 10, area.width - 4, area.height - 20};
      for (int op = 1; op <= 6; ++op)
      {
        const bool carrier = widgets::Dx7IsCarrier(algorithm, op);
        const float level = Glide(animator, track, engine, ("op" + std::to_string(op) + ".output").c_str());
        const int x = bars.x + (op - 1) * (barWidth + 6);
        const int height = std::max(1, static_cast<int>(level * static_cast<float>(bars.height)));
        g.Dither(Rect{x, bars.y, barWidth, bars.height}, kFaint, 2);
        const float flicker = carrier && activity > 0.02f ? activity * (0.5f + 0.5f * std::sin(static_cast<float>(now) / 40.0f + static_cast<float>(op))) : 0.0f;
        g.Dither(Rect{x, bars.y + bars.height - height, barWidth, height}, carrier ? kInk : kDim,
                 carrier ? kDitherSolid - static_cast<int>(flicker * 4.0f) : kDitherLevelOn - 4);
        g.HLine(x, bars.y + bars.height - height, barWidth, carrier ? kInk : kDim);
        const std::string label = std::to_string(op) + (carrier ? "C" : "M");
        g.Text(x + (barWidth - Gfx::Measure(label)) / 2, bars.y + bars.height + 2, label, carrier ? kInk : kDim);
      }
      break;
    }
    case PageGraphic::Adsr:
    {
      const float sustain = Glide(animator, track, engine, "env.sustain");
      widgets::Envelope(g, Rect{area.x + 2, area.y + 4, area.width - 4, area.height - 6},
                        {{0.05f + Glide(animator, track, engine, "env.attack"), 0.05f + Glide(animator, track, engine, "env.decay"), 0.02f, 0.7f,
                          0.05f + Glide(animator, track, engine, "env.release")}},
                        {{1.0f, sustain, sustain, sustain, 0.0f}}, kInk);
      // While notes sound, a play head runs along the envelope.
      if (activity > 0.02f)
      {
        const float position = std::fmod(static_cast<float>(now) / 900.0f, 1.0f);
        g.VLine(area.x + 2 + static_cast<int>(position * static_cast<float>(area.width - 4)), area.y + 4, area.height - 6, kTab);
      }
      break;
    }
    case PageGraphic::Filter:
    {
      const bool sampler = host.EngineKind(track) == synth::SynthEngineKind::Sampler;
      const float cutoff = Glide(animator, track, engine, sampler ? "filter.cutoff" : "vcf.cutoff", 1.0f);
      const float resonance = Glide(animator, track, engine, sampler ? "filter.resonance" : "vcf.resonance");
      // Resonance shimmers while the track sounds.
      const float shimmer = activity > 0.02f ? 0.04f * std::sin(static_cast<float>(now) / 45.0f) * activity : 0.0f;
      widgets::FilterCurve(g, Rect{area.x + 2, area.y + 4, area.width - 4, area.height - 6}, cutoff, std::max(0.0f, resonance + shimmer), kInk);
      break;
    }
    case PageGraphic::Oscillator:
    {
      // A running oscilloscope: the waveform scrolls, faster while the track sounds.
      const float speed = activity > 0.02f ? 1.0f / 380.0f : 1.0f / 2400.0f;
      const float phase = std::fmod(static_cast<float>(now) * speed, 1.0f);
      const bool pulse = EngineRaw(engine, "dco.pulse", 1) != 0;
      widgets::OscillatorScope(g, Rect{area.x + 2, area.y + 2, area.width - 4, area.height - 4}, EngineRaw(engine, "dco.saw", 1) != 0,
                               pulse ? Glide(animator, track, engine, "dco.pwm") : -1.0f, Glide(animator, track, engine, "dco.sub"),
                               Glide(animator, track, engine, "dco.noise"), Glide(animator, track, engine, "lfo.rate"),
                               activity > 0.02f ? kTab : kInk, phase);
      break;
    }
    case PageGraphic::Lfo:
    {
      // The LFO itself: shape, speed (cycles across the graph) and the delayed fade-in; it scrolls
      // at a speed that follows the rate.
      const bool dx7 = host.EngineKind(track) == synth::SynthEngineKind::Dx7;
      const float rate = Glide(animator, track, engine, dx7 ? "global.lfo_speed" : "lfo.rate");
      const float delay = dx7 ? 0.0f : 0.6f * Glide(animator, track, engine, "lfo.delay");
      const float depth = dx7 ? std::max(0.25f, std::max(Glide(animator, track, engine, "global.lfo_pitch"), Glide(animator, track, engine, "global.lfo_amp")))
                              : 1.0f;
      const int wave = dx7 ? EngineRaw(engine, "global.lfo_wave") : 0;
      const float cycles = 1.0f + rate * 7.0f;
      const float phase = std::fmod(static_cast<float>(now) * (0.0002f + rate * 0.0012f), 1.0f);
      const Rect graph{area.x + 2, area.y + 9, area.width - 4, area.height - 11};
      g.Text(area.x + 2, area.y, dx7 ? "LFO > PITCH / AMP" : "LFO > DCO / VCF", kDim);
      widgets::LfoShape(g, graph, wave, cycles, delay, depth, phase, activity > 0.02f ? kTab : kInk);
      break;
    }
    case PageGraphic::FxChain:
      DrawFxChain(g, controller, animator, area);
      break;
    case PageGraphic::Keys:
    {
      // The pads' two octaves: scale tones dotted, the root barred, held chords lit.
      const int base = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + std::clamp(state.keyboard.octave, -3, 3) * 12;
      DrawMiniPiano(g, area, controller, animator, base - ((base % 12) + 12) % 12, true);
      break;
    }
    case PageGraphic::Chord:
    {
      // What one key of this track plays: the chord on the root, then the chords of every scale degree.
      const KeyboardSettings& keys = state.keyboard;
      const ChordSettings& shape = state.chords[track];
      const int base = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + std::clamp(keys.octave, -3, 3) * 12;
      const int root = base - ((base % 12) + 12) % 12 + keys.root;
      std::array<std::uint8_t, kMaxPadNotes> notes{};
      const std::size_t count = PadNotes(keys, shape, synth::SynthEngineKind::Juno, root, notes);
      std::array<bool, 128> marked{};
      std::string line = NoteName(root) + " PLAYS ";
      for (std::size_t index = 0; index < count; ++index)
      {
        marked[notes[index]] = true;
        line += (index == 0 ? "" : " ") + NoteName(notes[index]);
      }
      g.TextFit(area.x + 2, area.y + 2, area.width - 4, line, kInk);
      // The scale's own chords (stacked thirds), I to VII.
      KeyboardSettings triads = keys;
      triads.lock = ScaleLock::Off;
      const ChordSettings triadShape{ChordType::Triad, 0};
      std::string degrees;
      for (int semitone = 0, degree = 0; semitone < 12 && degree < 7; ++semitone)
      {
        if (!InScale(keys, root + semitone)) continue;
        ++degree;
        std::array<std::uint8_t, kMaxPadNotes> triad{};
        if (PadNotes(triads, triadShape, synth::SynthEngineKind::Juno, root + semitone, triad) < 3) continue;
        const int third = triad[1] - triad[0];
        const int fifth = triad[2] - triad[0];
        // Uppercase font: minor is "-", as in lead sheets.
        const char* quality = third == 3 && fifth == 6 ? "DIM" : third == 3 ? "-" : fifth == 8 ? "+" : "";
        degrees += std::string(degrees.empty() ? "" : " ") + kNoteNames[static_cast<std::size_t>((root + semitone) % 12)] + quality;
      }
      g.TextFit(area.x + 2, area.y + 12, area.width - 4, "IN KEY  " + degrees, kDim);
      // The pads' two octaves from the root's C: every chord of the octave fits on them.
      DrawMiniPiano(g, Rect{area.x, area.y + 24, area.width, area.height - 24}, controller, animator, root - keys.root, true, &marked);
      break;
    }
    case PageGraphic::Arp:
    {
      // The order this track's arpeggiator plays: the held notes (or the root chord) across the
      // range, as bars by pitch; the step playing now is white.
      const auto seq = host.Sequencer().BuildUiState(track);
      const synth::ArpSettings arp = host.Sequencer().GetArpSettings(track);
      if (arp.latch) g.Text(area.x + area.width - Gfx::Measure("HOLD") - 2, area.y + 1, "HOLD", TrackColor(track));
      std::vector<int> held;
      if (seq.arpActive)
        for (std::size_t index = 0; index < seq.arpNoteCount; ++index) held.push_back(seq.arpNotes[index]);
      if (held.empty())
      {
        const KeyboardSettings& keys = state.keyboard;
        const int base = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + keys.root;
        std::array<std::uint8_t, kMaxPadNotes> notes{};
        const std::size_t count = PadNotes(keys, state.chords[track], synth::SynthEngineKind::Juno, base, notes);
        for (std::size_t index = 0; index < count; ++index) held.push_back(notes[index]);
        if (held.size() < 2) held = {base, base + 4, base + 7};
      }
      std::vector<int> pool;
      for (int octave = 0; octave < std::max<int>(1, arp.octaves); ++octave)
        for (const int note : held) pool.push_back(note + octave * 12);
      std::vector<int> order = pool;
      if (arp.mode != synth::ArpMode::Played) std::sort(order.begin(), order.end());
      if (arp.mode == synth::ArpMode::Down) std::reverse(order.begin(), order.end());
      if (arp.mode == synth::ArpMode::UpDown && order.size() > 2)
        for (std::size_t index = order.size() - 2; index > 0; --index) order.push_back(order[index]);
      if (arp.mode == synth::ArpMode::Random)
        for (std::size_t index = 0; index < order.size(); ++index) std::swap(order[index], order[(index * 7 + 3) % order.size()]);
      const int low = *std::min_element(pool.begin(), pool.end());
      const int high = *std::max_element(pool.begin(), pool.end());
      const Rect lane{area.x + 2, area.y + 10, area.width - 4, area.height - 12};
      const int steps = 16;
      const int width = lane.width / steps;
      const bool off = arp.mode == synth::ArpMode::Off;
      g.Text(area.x + 2, area.y, off ? "ARP OFF" : std::string(synth::kArpModeNames[static_cast<std::size_t>(arp.mode)]) + "  " +
                                       synth::kArpRateNames[std::min<std::size_t>(arp.rate, synth::kArpRateCount - 1)] + "  GATE " + std::to_string(arp.gate) + "%",
             off ? kFaint : kDim);
      for (int step = 0; step < steps; ++step)
      {
        const int note = order[static_cast<std::size_t>(step) % order.size()];
        const int height = 3 + (note - low) * (lane.height - 4) / std::max(1, high - low);
        const int barWidth = std::max(1, (width - 1) * std::max<int>(10, arp.gate) / 100);
        const bool now = seq.arpActive && static_cast<int>(seq.arpTick % static_cast<std::uint32_t>(steps)) == step;
        const Rect bar{lane.x + step * width, lane.y + lane.height - height, barWidth, height};
        if (off) g.Dither(bar, kFaint, 6);
        else g.Fill(bar, now ? kInk : TrackColor(track));
      }
      break;
    }
    case PageGraphic::System:
      DrawSystemInfo(g, controller, animator, area);
      break;
    case PageGraphic::Waveform:
    {
      const auto sampler = host.Sampler().BuildUiState(track);
      const Rect graph{area.x + 2, area.y + 10, area.width - 4, area.height - 10};
      const bool chop = host.EngineKind(track) == synth::SynthEngineKind::Chop;
      // The chop draws the trim the SETTING page keeps, magnified around the point being edited: the
      // window narrows and the mark stays in the middle instead of the waveform sitting still while
      // the mark runs across it.
      const ChopView view = chop ? ChopWindow(controller, engine) : ChopView{};
      // A trimmed file is drawn through a window even at 1x; the tag is only for the magnifier.
      const bool windowed = chop && (view.span < 0.9995f || view.start > 0.0005f);
      const int magnify = chop ? controller.ChopZoomFactor() : 1;
      // Room for the worst case an int can print, so the format cannot be truncated: the magnifier only
      // ever asks for X2 or X4, but the compiler checks the type, not the range.
      char tag[12] = "";
      if (magnify > 1)
      {
        std::snprintf(tag, sizeof(tag), "X%d", magnify);
        g.Text(area.x + area.width - Gfx::Measure(tag) - 2, area.y, tag, TrackColor(track));
      }
      std::string title = sampler.hasSample ? sampler.sampleName : std::string("NO SAMPLE - FILE OR REC");
      if (sampler.hasSample && sampler.sampleRate > 0)
      {
        char seconds[16];
        std::snprintf(seconds, sizeof(seconds), "  %.2fS", static_cast<double>(sampler.frameCount) / static_cast<double>(sampler.sampleRate));
        title += seconds;
      }
      g.TextFit(area.x + 2, area.y, area.width - 4 - (magnify > 1 ? 22 : 0), title,
                sampler.hasSample ? kDim : ((now / 600) % 2 ? kFaint : kDim));
      const float cursor = sampler.frameCount > 0 ? static_cast<float>(sampler.previewCursorFrame) / static_cast<float>(sampler.frameCount) : 0.0f;
      // A chopped file has no trim window of its own: the whole sample plays, cut by its slices.
      if (windowed)
        widgets::WaveformWindow(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                                view.start, view.start + view.span, cursor, sampler.previewing, 0.5f);
      else if (chop)
        widgets::Waveform(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                          0.0f, 1.0f, cursor, sampler.previewing, 0.5f);
      else
        widgets::Waveform(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                          chop ? 0.0f : Glide(animator, track, engine, "sample.start"),
                          chop ? 1.0f : Glide(animator, track, engine, "sample.end", 1.0f), cursor, sampler.previewing,
                          chop ? 0.5f : 1.0f);
      // The slice marks belong to the editing page; the playhead follows the sound on both.
      if (chop && engine != nullptr) DrawChopWave(g, controller, engine, track, graph, now, controller.ChopSliceMarkers(), view);
      break;
    }
    case PageGraphic::ChopTrim:
    {
      // The whole file, from edge to edge: what the sample browser gave the track, and the trim the
      // slices live inside. Coarse by design, so it never needs the magnifier.
      const auto sampler = host.Sampler().BuildUiState(track);
      const Rect graph{area.x + 2, area.y + 10, area.width - 4, area.height - 10};
      std::string title = sampler.hasSample ? sampler.sampleName : std::string("NO SAMPLE - FILE OR REC");
      if (sampler.hasSample && sampler.sampleRate > 0)
      {
        char seconds[16];
        std::snprintf(seconds, sizeof(seconds), "  %.2fS",
                      static_cast<double>(sampler.frameCount) / static_cast<double>(sampler.sampleRate));
        title += seconds;
      }
      g.TextFit(area.x + 2, area.y, area.width - 4, title,
                sampler.hasSample ? kDim : ((now / 600) % 2 ? kFaint : kDim));
      const float cursor = sampler.frameCount > 0 ? static_cast<float>(sampler.previewCursorFrame) / static_cast<float>(sampler.frameCount) : 0.0f;
      widgets::Waveform(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0, 0.0f, 1.0f,
                        cursor, sampler.previewing, 0.5f);
      if (engine != nullptr) DrawChopTrim(g, engine, track, graph);
      break;
    }
    case PageGraphic::DrumPads:
      DrawPads(g, controller, animator, area, engine);
      break;
    case PageGraphic::None:
    default:
      break;
  }
}

// A page of slots (INSTR pages and SETUP): parameter cells with value bars, action cells, the
// gliding focus frame and the page graphic. `current` animates edits and presses.
void DrawSlotPage(Gfx& g, const Controller& controller, Animator& animator, const InstrumentPage& page, std::uint8_t focus, bool current,
                  std::uint32_t focusKey)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  for (std::size_t slotIndex = 0; slotIndex < kGridCount; ++slotIndex)
  {
    const Slot& slot = page.slots[slotIndex];
    const Rect cell = Cell(slotIndex / kGridColumns, slotIndex % kGridColumns);
    // The chop's magnifier is a shortcut, not a cell: its three keys sit over the waveform the page is
    // read for, so they keep the LEDs and draw nothing at all.
    if (slot.action == SlotAction::ChopZoom1 || slot.action == SlotAction::ChopZoom2 || slot.action == SlotAction::ChopZoom4) continue;
    if (slot.IsAction())
    {
      widgets::ActionCell(g, cell, slot.label, current && state.gridDown[slotIndex], kDim);
      if (current) GridRipple(g, animator, slotIndex, kInk);
      continue;
    }
    if (!slot.IsParameter()) continue;
    synth::SynthParameterInfo info;
    std::string text;
    if (!controller.SlotValue(slot, info, text))
    {
      widgets::ParamCell(g, cell, slot.label, "--", -1.0f, CellStyle::Disabled);
      continue;
    }
    float fraction = ParamFraction(info);
    if (fraction >= 0.0f)
      fraction = animator.Smooth(Animator::Key(slot.id, static_cast<std::uint32_t>(state.track) * 31u + 7u), fraction, 60.0f);
    const float flash = current ? fx::Decay(now, animator.ValueChangedAt(slotIndex), 320) : 0.0f;
    widgets::ParamCell(g, cell, slot.label, text, fraction, slotIndex == focus ? CellStyle::Focused : CellStyle::Normal, flash, now, false);
    if (current) GridRipple(g, animator, slotIndex, kInk);
  }
  // The focus frame glides from cell to cell instead of jumping.
  if (page.slots[focus].IsParameter())
  {
    const Rect target = Cell(focus / kGridColumns, focus % kGridColumns);
    const int x = static_cast<int>(std::lround(animator.Smooth(focusKey, static_cast<float>(target.x), 50.0f)));
    const int y = static_cast<int>(std::lround(animator.Smooth(focusKey + 1, static_cast<float>(target.y), 50.0f)));
    fx::MarchingAnts(g, Rect{x - 1, y - 1, target.width + 2, target.height + 2}, kInk, now);
  }
  if (page.graphic != PageGraphic::None) DrawGraphic(g, controller, animator, page);
  for (std::size_t slotIndex = 0; slotIndex < kGridCount; ++slotIndex)
  {
    if (!page.slots[slotIndex].Empty() || InArea(page.area, slotIndex)) continue;
    const Rect cell = Cell(slotIndex / kGridColumns, slotIndex % kGridColumns);
    g.Pixel(cell.x + cell.width / 2, cell.y + cell.height / 2, kShade);
  }
}

void InstrumentScene(Gfx& g, const Controller& controller, Animator& animator, std::uint8_t pageIndex)
{
  const InstrumentPageList list = controller.InstrumentPages();
  const std::size_t index = std::min<std::size_t>(pageIndex, list.count - 1);
  const UiState& state = controller.State();
  const std::uint8_t focus = state.instrumentFocus[std::min<std::size_t>(index, state.instrumentFocus.size() - 1)];
  DrawSlotPage(g, controller, animator, list.pages[index], focus, index == controller.CurrentPage(),
               Animator::Key("instr-focus", static_cast<std::uint32_t>(state.track) * 16u + static_cast<std::uint32_t>(index)));
}

// SEQ ROLL: the pattern as a piano roll. Steps run left to right (the GRID's 16 keys, row by row),
// pitch bottom to top; each note is a bar as long as it plays, its fill as strong as its velocity.
// Two GRID pages share the screen - the one being edited bright, the one after it a dim guide - and a
// drum or chop track's gutter counts lanes (1 5 9 13) instead of naming notes.
void DrawPianoRoll(Gfx& g, const Controller& controller, Animator& animator, const synth::SequencerUiState& seq)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = controller.HostView().Sequencer();
  // The roll always shows two GRID pages - thirty-two steps - side by side. The page the keys edit is
  // bright; the one after it is drawn as a dim guide, and past the end of the pattern there is nothing
  // but the grid, so a four-bar pattern reads as one bar of steps and one bar of room. The keys still
  // write sixteen steps at a time: the page moves, the screen does not.
  const std::size_t length = std::max<std::size_t>(1, seq.tracks[state.track].stepCount);
  const std::size_t first = seq.firstViewStep;
  // A page is what the keys reach: sixteen steps on every track. The lane roll draws the page after it
  // as well (see `span`), dimmed, but that half is not part of what is being edited.
  const std::size_t page = std::clamp<std::size_t>(length > first ? length - first : 1, 1, kGridCount);
  const Color trackColor = TrackColor(state.track);
  constexpr int kGutter = 20;
  const int x0 = kContentX + kGutter;
  // Two rolls share this function. A drum kit's or a chop's rows are lanes, and its screen is two GRID
  // pages wide - the one being edited bright, the one after it a guide - with a ruler in the gutter. A
  // melodic track keeps the piano roll it always had: the page's steps filling the width, note names
  // down the side, a strip of step cells under it.
  const bool drums = controller.DrumTrack();
  const bool chop = controller.HostView().EngineKind(state.track) == synth::SynthEngineKind::Chop;
  const bool lanes = drums || chop;
  const std::size_t span = lanes ? synth::kSequencerMaxViewSteps : page;
  const int column = (kContentWidth - kGutter) / static_cast<int>(span);
  const int pageWidth = column * static_cast<int>(page);
  const int fullWidth = column * static_cast<int>(span);
  // A lane roll's window is two pages wide and only moves every second page. Inside it the page being
  // edited is the bright half - the left one on pages 1, 3, 5 ..., the right one on 2, 4, 6 ... - and
  // the other half is drawn dim: its notes stay on screen, they are just not what the keys write.
  const std::size_t windowFirst = lanes ? (first / kGridCount / 2) * (kGridCount * 2) : first;
  const std::size_t brightStart = lanes ? first - windowFirst : 0;
  const int brightX0 = x0 + static_cast<int>(brightStart) * column;
  const int brightWidth = column * static_cast<int>(page);
  const int top = kContentY + 1;
  constexpr int kStrip = 9;
  // A lane track gives the strip's nine pixels to the notes: the step the keys are on is already a lit
  // column of its roll.
  const int bottom = kContentBottom - (lanes ? 2 : kStrip + 2);

  // Every note of the pattern, to fit the pitch range around them.
  std::array<std::array<synth::SynthSequencer::StepNoteView, 8>, synth::kSequencerMaxViewSteps> notes{};
  std::array<std::size_t, synth::kSequencerMaxViewSteps> counts{};
  int low = 128, high = -1;
  for (std::size_t step = 0; step < span; ++step)
  {
    counts[step] = sequencer.StepNoteEvents(state.track, windowFirst + step, notes[step]);
    for (std::size_t index = 0; index < counts[step]; ++index)
    {
      low = std::min<int>(low, notes[step][index].note);
      high = std::max<int>(high, notes[step][index].note);
    }
  }
  // Notes of the page before that still sound on this one.
  std::array<std::array<synth::SynthSequencer::StepNoteView, 8>, synth::kSequencerMaxViewSteps> carried{};
  std::array<std::size_t, synth::kSequencerMaxViewSteps> carriedCounts{};
  if (windowFirst >= span)
  {
    for (std::size_t step = 0; step < span; ++step)
    {
      const std::size_t pulseStart = (windowFirst - span + step) * synth::kSequencerPulsesPerStep;
      std::array<synth::SynthSequencer::StepNoteView, 8> before{};
      const std::size_t count = sequencer.StepNoteEvents(state.track, windowFirst - span + step, before);
      for (std::size_t index = 0; index < count; ++index)
      {
        const std::size_t end = pulseStart + before[index].microstep * synth::kSequencerPulsesPerMicrostep + before[index].lengthPulses;
        if (end <= windowFirst * synth::kSequencerPulsesPerStep) continue;
        carried[step][carriedCounts[step]++] = before[index];
        low = std::min<int>(low, before[index].note);
        high = std::max<int>(high, before[index].note);
      }
    }
  }
  // The notes the pads hold (chord tones too): their rows light up, the rows a tapped step writes.
  // How hard: a drum's row lights as dense as its pad hits, a note's as a checker.
  std::array<std::uint8_t, 128> pressed{};
  int pressedLow = 128, pressedHigh = -1;
  for (std::size_t pad = 0; pad < state.padNote.size(); ++pad)
  {
    if (state.padNote[pad] == kNoNote || state.padTrack[pad] != state.track) continue;
    const std::uint8_t velocity = std::max<std::uint8_t>(state.padVelocity[pad], 1);
    const auto press = [&](std::uint8_t note) {
      pressed[note & 0x7F] = std::max(pressed[note & 0x7F], velocity);
      pressedLow = std::min<int>(pressedLow, note & 0x7F);
      pressedHigh = std::max<int>(pressedHigh, note & 0x7F);
    };
    press(state.padNote[pad]);
    for (std::size_t voice = 0; voice < state.padChordCount[pad]; ++voice) press(state.padChord[pad][voice]);
  }
  // A melodic track's roll keeps every key the pads play at this octave, used or not (nothing folds
  // away), widened for notes of the page and held notes outside it. A drum track's fits its hits.
  int padLow = 128, padHigh = -1;
  if (!drums)
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = kPadFirstColumn; column < kPadFirstColumn + kPadColumns; ++column)
      {
        int note = 0;
        if (!controller.PadNoteFor(row, column, note)) continue;
        padLow = std::min(padLow, note);
        padHigh = std::max(padHigh, note);
      }
  if (padHigh >= 0)
  {
    low = std::min(high < 0 ? padLow : low, padLow);
    high = std::max(high, padHigh);
  }
  else if (high < 0)
  {
    low = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + std::clamp(state.keyboard.octave, -3, 3) * 12;
    high = low + 12;
  }
  else
  {
    low -= 2;
    high += 2;
    while (high - low < 12)
    {
      --low;
      ++high;
    }
  }
  // A held note outside the range widens it, so its row is on screen.
  if (pressedHigh >= 0)
  {
    low = std::min(low, pressedLow - (padHigh >= 0 ? 0 : 1));
    high = std::max(high, pressedHigh + (padHigh >= 0 ? 0 : 1));
  }
  // At most 41 rows of 2 pixels: too wide a range keeps the 41 around the pads' (or the notes') middle.
  if (high - low > 40)
  {
    const int middle = padHigh >= 0 ? (padLow + padHigh) / 2 : (low + high) / 2;
    low = std::clamp(middle - 20, low, high - 40);
    high = low + 40;
  }
  low = std::max(0, low);
  high = std::min(127, high);
  const int rows = high - low + 1;
  // A lane track has a row per voice, not per note: a chop counts its slices, a kit its drums - and a
  // kit's drums are not neighbours on the keyboard (36, 38, 42, 46, 39, 43, 56, 70), so counting
  // semitones would leave empty rows between them. So many rows as the track has voices.
  const int lanes_ = lanes ? (chop ? static_cast<int>(controller.ChopSliceCount()) : static_cast<int>(kDrumNotes.size())) : rows;
  const int rowHeight = std::max(2, (lanes ? kStepBand : bottom - top) / lanes_);
  const int height = rowHeight * lanes_;
  const int y0 = lanes ? kContentY + (kContentHeight - height) / 2 : top + (bottom - top - height) / 2;
  // Which row a note sits on: a chop counts down the page (slice 1 first), a kit in the order the pads
  // are laid out, and a piano roll keeps high notes at the top.
  const auto laneRow = [&](int note) {
    if (chop)
    {
      const int lane = note - static_cast<int>(synth::kChopFirstNote);
      return lane >= 0 && lane < lanes_ ? lane : -1;
    }
    for (std::size_t lane = 0; lane < kDrumRowPad.size(); ++lane)
      if (kDrumNotes[kDrumRowPad[lane]] == note) return static_cast<int>(lane);
    return -1;
  };
  const auto laneNote = [&](int row) {
    return chop ? static_cast<int>(synth::kChopFirstNote) + row : static_cast<int>(kDrumNotes[kDrumRowPad[static_cast<std::size_t>(row)]]);
  };
  auto rowY = [&](int note) {
    if (lanes)
    {
      const int row = laneRow(note);
      return y0 + (std::max)(0, row) * rowHeight;
    }
    return y0 + (high - note) * rowHeight;
  };
  // A lane is as tall as the rows allow, and a bar fills it as far as the hit is hard: a six-pixel
  // lane reads as three heights, an eight-slice track's fat rows as ten, and both agree.
  const auto noteBar = [](const Rect& room, std::uint8_t velocity) {
    const int height = BarHeight((std::max)(1, room.height), velocity);
    return Rect{room.x, room.y + room.height - height, room.width, height};
  };
  // The lane roll's frame carries the same white rule as the four-lane dividers, so its grid is closed
  // at the top; the ruler is capped by a tick of its own.
  if (lanes)
  {
    g.HLine(x0, y0 - 1, fullWidth, kInk);
    g.HLine(x0 - 6, y0 - 1, 7, kDim);
  }

  // A lane track's gutter counts its lanes instead of naming notes - 1 5 9 13 down a chop - and a kit
  // names them, BD SD CH, the way the step matrix does.
  for (int row = 0; row < (lanes ? lanes_ : rows); ++row)
  {
    const int lane = lanes ? row : 0;
    const int note = lanes ? laneNote(row) : low + row;
    const int y = lanes ? rowY(note) : y0 + row * rowHeight;
    const int lineY = y + rowHeight - 1;
    if (lanes)
    {
      // The grid is lines, not a halftone: a hairline under every lane, a bright white one every fourth
      // to group them. The page being edited is drawn brighter than the half that is only context.
      // A chop counts down the page, so its groups start at slice 1 (row one) and close under slice 4;
      // the drums count up from the floor and keep their own way.
      const bool divider = chop ? lane % 4 == 3 : lane % 4 == 0;
      const bool counted = chop ? lane % 4 == 0 : true;
      const Color hairline = divider ? kInk : kFaint;
      g.HLine(x0, lineY, fullWidth, divider ? kInk : kShade);
      g.HLine(brightX0, lineY, brightWidth, hairline);
      // The gutter is a ruler beside it: a tick a lane, a longer one where the counting lands.
      g.HLine(x0 - (divider ? 6 : 3), lineY, divider ? 7 : 4, divider ? kDim : kFaint);
      if (counted && rowHeight * 4 >= 10)
      {
        const std::string text = chop ? std::to_string(lane + 1) : std::string(DrumName(note));
        g.Text(kContentX, y + rowHeight / 2 - 4, text, kFaint, Font::Micro);
      }
      continue;
    }
    // The piano roll: black keys shaded, every C named.
    const int pitch = note % 12;
    const bool black = pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10;
    if (black) g.Dither(Rect{x0, y, fullWidth, rowHeight}, kShade, 3);
    if (pitch == 0)
    {
      g.DottedHLine(x0, lineY, fullWidth, kShade, 2);
      if (rowHeight >= 3 || note % 24 == 0)
        g.Text(kContentX, std::max(top, y + rowHeight - 7), NoteName(note), kFaint, Font::Micro);
    }
  }
  // Held notes: their rows lit across the page (a checker; a drum as dense as its pad hits), their
  // names (a drum's) inverse in the gutter.
  for (int row = 0; row < (lanes ? lanes_ : rows); ++row)
  {
    const int note = lanes ? laneNote(row) : low + row;
    const std::uint8_t held = pressed[static_cast<std::size_t>(note)];
    if (held == 0) continue;
    const int y = lanes ? rowY(note) : y0 + row * rowHeight;
    g.Dither(Rect{brightX0, y, brightWidth, rowHeight}, kInk, drums ? DrumDensity(held) : 8);
    const int labelY = std::clamp(y + rowHeight / 2 - 4, top, bottom - 8);
    const std::string_view drum = drums ? DrumName(note) : std::string_view{};
    g.Fill(Rect{kContentX, labelY, kGutter - 2, 8}, kInk);
    const std::string label = chop ? std::to_string(row + 1) : std::string(drum.empty() ? NoteName(note) : std::string(drum));
    g.Text(kContentX + 1, labelY + 1, label, kBlack, Font::Micro);
  }
  // Beats, the selection and the playhead.
  if (lanes)
  {
    for (std::size_t step = 0; step <= span; ++step)
    {
      const bool inside = step >= brightStart && step <= brightStart + page;
      const bool beat = (windowFirst + step) % 4 == 0;
      const Color color = inside ? (beat ? kDim : kFaint) : (beat ? kFaint : kShade);
      // One pixel higher than the rows: that is what breaks the frame line into the same dashes as the
      // dividers (a column line through a white rule, a gap where the rule shows through).
      g.VLine(x0 + static_cast<int>(step) * column, y0 - 1, height + 1, color);
    }
    for (std::size_t step = 0; step < page; ++step)
      if (seq.steps[step].held) g.Dither(Rect{brightX0 + static_cast<int>(step) * column + 1, y0, column - 1, height}, kInk, 3);
  }
  else
  {
    // The piano roll keeps the dotted beat marks it always had.
    for (std::size_t step = 0; step <= span; step += 4)
      for (int y = y0; y < y0 + height; y += 2)
        g.Pixel(x0 + static_cast<int>(step) * column, y, step % 16 == 0 ? kDim : kShade);
    for (std::size_t step = 0; step < span; ++step)
      if (seq.steps[step].held) g.Dither(Rect{x0 + static_cast<int>(step) * column, y0, column, height}, kInk, 3);
  }
  // Two whites, and they mean different things: every note of a selected step is white and stays white,
  // while the one note the scope is on blinks between that white and the dark of the cell - it is the
  // note ADD / SUB edits, so it is the only thing on the page that moves.
  const bool scopeBlink = ((animator.Now() / 250u) & 1u) == 0;
  const auto scopedNote = [&](std::size_t step, int note) {
    return seq.hasHeldSteps && seq.scopedNote >= 0 && first + step == seq.primaryHeldStep && note == seq.scopedNote;
  };
  // A note, in whichever roll it is being drawn. The lane roll whitens every note of a selected step
  // and blinks the scoped one over that white; the piano roll keeps its old reading - the scoped note
  // blinking white, a selected step's or a lit row's in the track's colour, cut out of the light.
  // Scope narrows the highlight: with a note scoped, the rest of the held column goes back to the
  // track's own colour, so the one thing on the page that moves - the scoped note, blinking white -
  // is also the only white.
  const bool scoping = seq.hasHeldSteps && seq.scopedNote >= 0;
  const auto drawBar = [&](const Rect& bar, bool held, bool scoped, bool lit) {
    if (lanes)
    {
      if (scoped)
      {
        // Track colour, blinking white: it is the note ADD / SUB edits.
        g.Fill(Rect{bar.x, bar.y - 1, bar.width, bar.height + 2}, kBlack);
        g.Fill(bar, scopeBlink ? kInk : trackColor);
        return;
      }
      if (held && !scoping)
      {
        // Every note of a selected step is white, cut out of the lit column.
        g.Fill(Rect{bar.x, bar.y - 1, bar.width, bar.height + 2}, kBlack);
        g.Fill(bar, kInk);
        return;
      }
      if (held || lit)
      {
        // Out of the light but in its own colour: the selected step, or a row a pad is holding.
        g.Fill(Rect{bar.x, bar.y - 1, bar.width, bar.height + 2}, kBlack);
        g.Fill(bar, trackColor);
        return;
      }
      g.Fill(bar, trackColor);
      return;
    }
    if (scoped && scopeBlink)
    {
      g.Fill(Rect{bar.x, bar.y - 1, bar.width, bar.height + 2}, kBlack);
      g.Fill(bar, kInk);
      return;
    }
    if (lit || held)
    {
      g.Fill(Rect{bar.x, bar.y - 1, bar.width, bar.height + 2}, kBlack);
      g.Fill(bar, trackColor);
      return;
    }
    g.Fill(bar, trackColor);
    g.Fill(Rect{bar.x, bar.y, 1, bar.height}, trackColor);
  };
  // The notes. Counted as drawn: the hint below is for a screen with nothing on it, and a stored
  // count can lag a write by a frame or two.
  int drawnNotes = 0;
  for (std::size_t step = 0; step < span; ++step)
  {
    const std::size_t pulseStart = step * synth::kSequencerPulsesPerStep;
    for (std::size_t index = 0; index < carriedCounts[step]; ++index)
    {
      const auto& note = carried[step][index];
      if (lanes ? laneRow(note.note) < 0 : (note.note < low || note.note > high)) continue;
      const int inPage = static_cast<int>(pulseStart + note.microstep * synth::kSequencerPulsesPerMicrostep + note.lengthPulses) - static_cast<int>(span * synth::kSequencerPulsesPerStep);
      const Rect room{x0 + (lanes ? 1 : 0), rowY(note.note), std::min(inPage * column / static_cast<int>(synth::kSequencerPulsesPerStep) - 1, fullWidth - 1), std::max(1, rowHeight - 1)};
      const Rect bar = lanes ? noteBar(room, note.velocity) : VelocityBar(room, note.velocity);
      drawBar(bar, seq.steps[step].held, scopedNote(step, note.note), pressed[note.note & 0x7F] != 0);
      DrawRollNotches(g, bar, note.roll);
      ++drawnNotes;
    }
  }
  for (std::size_t step = 0; step < page; ++step)
  {
    // The page being edited sits at `brightStart` in the window, so its notes are the ones loaded for
    // that slot: the left half on an odd page, the right half on an even one.
    const std::size_t slot = lanes ? brightStart + step : step;
    for (std::size_t index = 0; index < counts[slot]; ++index)
    {
      const auto& note = notes[slot][index];
      if (lanes ? laneRow(note.note) < 0 : (note.note < low || note.note > high)) continue;
      // On a lane roll a bar sits inside its cell, one pixel in, so the grid's lines stay the box it
      // fills; the piano roll's bars keep their old place on the column.
      const int x = x0 + static_cast<int>(slot) * column +
                    static_cast<int>(note.microstep) * column / synth::kSequencerMicrostepCount + (lanes ? 1 : 0);
      const int width = std::max(1, static_cast<int>(note.lengthPulses) * column / synth::kSequencerPulsesPerStep - 1);
      const Rect room{x, rowY(note.note), std::min(width, x0 + fullWidth - (lanes ? 1 : 0) - x), std::max(1, rowHeight - 1)};
      const Rect bar = lanes ? noteBar(room, note.velocity) : VelocityBar(room, note.velocity);
      drawBar(bar, seq.steps[step].held, scopedNote(step, note.note), pressed[note.note & 0x7F] != 0);
      DrawRollNotches(g, bar, note.roll);
      ++drawnNotes;
    }
  }
  // The half that is not being edited keeps its notes, and in their own colour: on an odd page that is
  // what is coming, on an even page what was just played. Only the grid under them stays dim, so the
  // eye knows which half the keys write without the other half going grey. Past the end of the pattern
  // there is nothing to draw.
  for (std::size_t slot = 0; lanes && slot < span; ++slot)
  {
    if (slot >= brightStart && slot < brightStart + page) continue;
    if (windowFirst + slot >= length) break;
    for (std::size_t index = 0; index < counts[slot]; ++index)
    {
      const auto& note = notes[slot][index];
      if (lanes ? laneRow(note.note) < 0 : (note.note < low || note.note > high)) continue;
      const int x = x0 + static_cast<int>(slot) * column +
                    static_cast<int>(note.microstep) * column / synth::kSequencerMicrostepCount + 1;
      const int width = std::max(1, static_cast<int>(note.lengthPulses) * column / synth::kSequencerPulsesPerStep - 1);
      const Rect room{x, rowY(note.note), std::min(width, x0 + fullWidth - 1 - x), std::max(1, rowHeight - 1)};
      const Rect bar = noteBar(room, note.velocity);
      g.Fill(bar, trackColor);
      DrawRollNotches(g, bar, note.roll);
      ++drawnNotes;
    }
  }
  if (seq.playing)
  {
    const float head = seq.patternProgress * static_cast<float>(length) - static_cast<float>(windowFirst);
    if (head >= 0.0f && head < static_cast<float>(span)) g.VLine(x0 + static_cast<int>(head * static_cast<float>(column)), y0, height, kPlay);
  }
  // The GRID strip: the piano roll keeps it (the step cells under the notes), a lane roll does not.
  if (!lanes)
  {
    const int stripY = kContentBottom - kStrip;
    for (std::size_t step = 0; step < kGridCount; ++step)
    {
      if (step >= span) continue;
      const Rect cell{x0 + static_cast<int>(step) * column + 1, stripY, column - 2, kStrip - 2};
      const auto& info = seq.steps[step];
      if (info.held) g.Fill(cell, kInk);
      else if (info.playing && seq.playing) g.Fill(cell, kPlay);
      else if (info.hasEvents) g.Dither(cell, trackColor, 10);
      else g.Stroke(cell, kShade);
      if (step % 4 == 0) g.Text(cell.x, stripY - 8, std::to_string(first + step + 1), kFaint, Font::Micro);
    }
  }
  // An empty screen says how to fill it, unless a held note's row is to be seen.
  if (drawnNotes == 0 && pressedHigh < 0)
  {
    const Rect band{brightX0, y0 + height / 2 - 10, brightWidth, 20};
    g.Fill(band, kBlack);
    g.TextAligned(Rect{band.x, band.y, band.width, 10}, "HOLD A PAD + TAP STEPS", kDim, Font::Micro, Align::Center);
    g.TextAligned(Rect{band.x, band.y + 10, band.width, 10}, "TAP AGAIN = ROLL   FN CLEARS", (now / 600) % 2 ? kFaint : kShade, Font::Micro, Align::Center);
  }
}

// The STEP view of the note page, the drum machine's look for every track. Left, a row per drum (a drum
// track's kit) or per pitch (a melodic track: every key the pads play at this octave, used or not, as
// in the piano roll, nothing folded away) over the steps of the page, the whole beat at a glance;
// right, the GRID as it lies under the fingers, for the drum or note the pads chose. A row whose pad
// is down is lit across (a drum's as dense as the pad hits), the row a tapped step writes into; notes
// keep the track's color everywhere.
void DrawStepMachine(Gfx& g, const Controller& controller, Animator& animator, const synth::SequencerUiState& seq)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = controller.HostView().Sequencer();
  const std::size_t length = seq.tracks[state.track].stepCount;
  const std::size_t first = seq.firstViewStep;
  const Color trackColor = TrackColor(state.track);
  const bool drums = controller.DrumTrack();
  // A chop's rows are its slices, not pitches: it gets the drum machine's compact rows and the roll's
  // lane ruler down the side (1 5 9 13), so both screens count the same way.
  const bool chop = controller.HostView().EngineKind(state.track) == synth::SynthEngineKind::Chop;
  const bool lanes = drums || chop;
  const std::size_t sliceCount = chop ? std::max<std::size_t>(1, controller.ChopSliceCount()) : 0;
  const bool playingHere = seq.playing && seq.currentStep >= first && seq.currentStep < first + kGridCount;

  // How hard each note is held on the pads (0: not held), chord tones too.
  std::array<std::uint8_t, 128> heldVelocity{};
  for (std::size_t pad = 0; pad < state.padNote.size(); ++pad)
  {
    if (state.padNote[pad] == kNoNote || state.padTrack[pad] != state.track) continue;
    const std::uint8_t velocity = std::max<std::uint8_t>(state.padVelocity[pad], 1);
    const auto hold = [&](std::uint8_t note) { heldVelocity[note & 0x7F] = std::max(heldVelocity[note & 0x7F], velocity); };
    hold(state.padNote[pad]);
    for (std::size_t voice = 0; voice < state.padChordCount[pad]; ++voice) hold(state.padChord[pad][voice]);
  }
  std::array<std::array<synth::SynthSequencer::StepNoteView, 8>, kGridCount> notes{};
  std::array<std::size_t, kGridCount> counts{};
  for (std::size_t step = 0; step < kGridCount; ++step) counts[step] = sequencer.StepNoteEvents(state.track, first + step, notes[step]);
  // The drum or note the GRID shows: the one the pads chose last.
  const int picked = drums ? kDrumNotes[controller.DrumLane()] : (state.noteLane[state.track] == kNoNote ? -1 : state.noteLane[state.track]);

  // The rows. A melodic track's: from the highest pitch down, the pads' range at this octave widened
  // for notes of the page, held and chosen notes outside it; at most 41 rows, around the pads' middle.
  int low = 0, high = 0;
  int rows = drums ? static_cast<int>(kDrumNotes.size()) : static_cast<int>(sliceCount);
  if (!lanes)
  {
    int padLow = 128, padHigh = -1;
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = kPadFirstColumn; column < kPadFirstColumn + kPadColumns; ++column)
      {
        int note = 0;
        if (!controller.PadNoteFor(row, column, note)) continue;
        padLow = std::min(padLow, note);
        padHigh = std::max(padHigh, note);
      }
    low = padHigh >= 0 ? padLow : 128;
    high = padHigh;
    const auto widen = [&](int note) {
      low = std::min(low, note);
      high = std::max(high, note);
    };
    for (std::size_t step = 0; step < kGridCount; ++step)
      for (std::size_t slot = 0; slot < counts[step]; ++slot) widen(notes[step][slot].note);
    for (int note = 0; note < 128; ++note)
      if (heldVelocity[static_cast<std::size_t>(note)] != 0) widen(note);
    if (picked >= 0) widen(picked);
    if (high < 0)
    {
      low = static_cast<int>(synth::kHomePerformanceBaseMidiNote) + std::clamp(state.keyboard.octave, -3, 3) * 12;
      high = low + 12;
    }
    if (high - low > 40)
    {
      const int middle = padHigh >= 0 ? (padLow + padHigh) / 2 : (low + high) / 2;
      low = std::clamp(middle - 20, low, high - 40);
      high = low + 40;
    }
    rows = high - low + 1;
  }
  const auto rowOf = [&](int note) {
    if (chop)
    {
      // Slice 1 is the first row: the counting runs down the page, the way the pads are laid out.
      const int slice = note - static_cast<int>(synth::kChopFirstNote);
      if (slice < 0 || slice >= rows) return -1;
      return slice;
    }
    if (!drums) return note < low || note > high ? -1 : high - note;
    for (std::size_t row = 0; row < kDrumRowPad.size(); ++row)
      if (kDrumNotes[kDrumRowPad[row]] == note) return static_cast<int>(row);
    return -1;
  };
  const auto noteOf = [&](int row) {
    if (chop) return static_cast<int>(synth::kChopFirstNote) + row;
    return drums ? static_cast<int>(kDrumNotes[kDrumRowPad[static_cast<std::size_t>(row)]]) : high - row;
  };
  const int chosen = picked >= 0 ? rowOf(picked) : -1;

  const int label = drums ? 14 : 20;
  // The GRID on the right decides the split: it is four squares again, and what it leaves the matrix
  // has. A square is the width the cells had, less three.
  constexpr int kGap = kStepGap;
  constexpr int kNameHeight = kStepNameHeight;
  constexpr int kSquare = kStepSquare;
  const int left = kContentRight - kStepBand;
  const int x0 = kContentX + label;
  const int column = std::max(3, (left - 5 - x0) / static_cast<int>(kGridCount));
  const int matrixWidth = column * static_cast<int>(kGridCount);
  const int rowHeight = std::max(2, kStepBand / rows);
  const int matrixHeight = rowHeight * rows;
  const int top = kContentY + (kContentHeight - matrixHeight) / 2;
  const int steps = static_cast<int>(std::min<std::size_t>(kGridCount, length > first ? length - first : 0));

  // The chop and the kit share one screen: rows that are voices rather than pitches, a grid ruled a
  // step both ways, a ruler in the gutter and a square a step on the GRID.
  const bool kit = chop || drums;
  // Row backgrounds: black keys shaded (melodic), the chosen row, the rows whose pads are down.
  for (int row = 0; row < rows; ++row)
  {
    const int y = top + row * rowHeight;
    const int note = noteOf(row);
    const std::uint8_t held = heldVelocity[static_cast<std::size_t>(note)];
    const int pitch = note % 12;
    if (kit)
    {
      // The roll's ruler, turned on its side: a hairline a lane, a white one every fourth, and - on a
      // chop - the slice number where the counting lands, 1, 5, 9, 13 reading down. A kit names its
      // lanes instead, so it carries no numbers.
      const int lineY = y + rowHeight - 1;
      const bool divider = row % 4 == 3;
      const bool counted = chop && row % 4 == 0;
      g.HLine(x0, lineY, matrixWidth, divider ? kInk : kFaint);
      g.HLine(x0 - (divider ? 6 : 3), lineY, divider ? 7 : 4, divider ? kInk : kFaint);
      if (counted && rowHeight * 4 >= 10) g.Text(kContentX, y + rowHeight / 2 - 4, std::to_string(row + 1), kDim, Font::Micro);
      if (held != 0) g.Dither(Rect{x0, y, matrixWidth, rowHeight - 1}, kInk, drums ? DrumDensity(held) : 8);
      else if (row == chosen) g.Dither(Rect{x0, y, matrixWidth, rowHeight - 1}, kShade, 8);
      continue;
    }
    if (!drums && (pitch == 1 || pitch == 3 || pitch == 6 || pitch == 8 || pitch == 10)) g.Dither(Rect{x0, y, matrixWidth, rowHeight}, kShade, 3);
    // Lit across: a note a checker.
    if (held != 0) g.Dither(Rect{x0, y, matrixWidth, rowHeight}, kInk, 8);
    else if (row == chosen) g.Dither(Rect{x0, y, matrixWidth, rowHeight}, kShade, 8);
    if (!drums && pitch == 0) g.DottedHLine(x0, y + rowHeight - 1, matrixWidth, kShade, 2);
  }
  // Beats (melodic: the rows are too thin for the dots of the drum rows), the selected steps' columns.
  if (!lanes)
    for (int step = 0; step <= steps; step += 4)
      for (int y = top; y < top + matrixHeight; y += 2) g.Pixel(x0 + step * column, y, step % 16 == 0 ? kDim : kShade);
  for (int step = 0; step < steps; ++step)
    if (seq.steps[static_cast<std::size_t>(step)].held) g.Dither(Rect{x0 + step * column, top, column, matrixHeight}, kInk, 3);
  // A chop's or a kit's matrix is a grid, not a set of lanes: the steps run down the screen too, so a
  // hit is a cell filled and the counting reads at a glance. The ruler's white lines - above the
  // first lane and under every fourth - go down last, over the columns, so a crossing does not cut
  // them.
  if (kit)
  {
    for (int step = 0; step <= steps; ++step)
      g.VLine(x0 + step * column, top - 1, matrixHeight + 1, step % 4 == 0 ? kDim : kFaint);
    for (int row = 0; row < rows; ++row)
      if (row % 4 == 3) g.HLine(x0 - 6, top + row * rowHeight + rowHeight - 1, matrixWidth + 6, kInk);
    g.HLine(x0 - 6, top - 1, matrixWidth + 6, kInk);
  }
  // Names: every drum; a melodic track's C rows, and the chosen and held notes inverse.
  for (int row = 0; row < rows; ++row)
  {
    const int y = top + row * rowHeight;
    const int note = noteOf(row);
    const bool lit = row == chosen || heldVelocity[static_cast<std::size_t>(note)] != 0;
    if (chop) continue;
    if (drums)
    {
      // A kit names its lanes - BD, SD, CH - where a chop counts its slices: the grid already says
      // which step is which, so no dots are needed to read an empty lane.
      if (lit) g.Fill(Rect{kContentX, y, label - 2, rowHeight - 1}, kInk);
      g.Text(kContentX + 1, y + (rowHeight - 6) / 2, kDrumNames[kDrumRowPad[static_cast<std::size_t>(row)]], lit ? kBlack : kDim, Font::Micro);
      continue;
    }
    if (!lit && note % 12 != 0) continue;
    const int labelY = std::clamp(y + rowHeight / 2 - 4, kContentY, kContentBottom - 8);
    if (lit) g.Fill(Rect{kContentX, labelY, label - 2, 8}, kInk);
    if (lit || rowHeight >= 3 || note % 24 == 0) g.Text(kContentX + 1, labelY + 1, NoteName(note), lit ? kBlack : kFaint, Font::Micro);
  }
  // The scoped note blinks in reverse here too: on the matrix that is the row of the one piece
  // ADD / SUB will edit, which the strip below does not say. Two blinks a second.
  const bool scopeBlink = ((animator.Now() / 250u) & 1u) == 0;
  const bool scoping = seq.hasHeldSteps && seq.scopedNote >= 0;
  const auto scopedNow = [&](std::size_t step, int note) {
    return scopeBlink && scoping && first + step == seq.primaryHeldStep && note == seq.scopedNote;
  };
  // The hits, in the track's color: on a lit or chosen row cut out of its light and stronger.
  for (int step = 0; step < steps; ++step)
  {
    const bool head = playingHere && seq.currentStep == first + static_cast<std::size_t>(step);
    for (std::size_t slot = 0; slot < counts[static_cast<std::size_t>(step)]; ++slot)
    {
      const auto& note = notes[static_cast<std::size_t>(step)][slot];
      const int row = rowOf(note.note);
      if (row < 0) continue;
      const int y = top + row * rowHeight;
      const int late = note.microstep;
      const int cellX = x0 + step * column;
      const int x = cellX + late * (column - 3) / (synth::kSequencerMicrostepCount - 1);
      // On the beat it fills the step; off it, a sliver where in the step it falls. A chop's or a
      // kit's block takes the cell's whole width - TIME is left to the GRID square - and stands on
      // its floor as tall as the hit is hard.
      const Rect room = kit ? Rect{cellX + 1, y, column - 1, std::max(2, rowHeight - 1)}
                            : lanes ? Rect{x, y + 2, late == 0 ? column - 1 : 2, std::max(2, rowHeight - 5)}
                                    : Rect{x, y, late == 0 ? column - 1 : 2, std::max(2, rowHeight - 1)};
      // Same language as the piano roll: how hard, by how tall, never under two pixels.
      const Rect mark = kit ? ChopBar(room, note.velocity) : VelocityBar(room, note.velocity);
      const std::uint8_t held = heldVelocity[note.note & 0x7F];
      const bool heldStep = seq.steps[static_cast<std::size_t>(step)].held;
      const bool strong = held != 0 || row == chosen || heldStep;
      if (strong && !kit) g.Fill(Rect{mark.x - 1, mark.y - 1, mark.width + 2, mark.height + 2}, kBlack);
      // The roll's two whites, on the matrix too: every note of a selected step is white and stays
      // white, and the one the scope is on blinks between that white and its own colour. Scope
      // narrows it - with a note scoped, the rest of the column goes back to the track's colour.
      if (scopedNow(static_cast<std::size_t>(step), note.note)) g.Fill(mark, kInk);
      else if (lanes && heldStep && !scoping) g.Fill(mark, kInk);
      else if (head) g.Fill(mark, kPlay);
      else g.Fill(mark, trackColor);
      // ROLL cuts the bar into the repeats it fires on, here as in the GRID square.
      if (kit) DrawChopCuts(g, mark, note.roll);
      else DrawRollNotches(g, mark, note.roll);
    }
  }
  if (playingHere)
    for (int y = top; y < top + matrixHeight; y += 2)
      g.Pixel(x0 + static_cast<int>(seq.currentStep - first) * column + 2, y, kPlay);
  // No words over an empty pattern here: the matrix is too narrow for them and they covered the names
  // (the piano roll, which has the width, keeps its hint).

  // The GRID: the same 4 x 4 as the keys, for the chosen drum, slice or note. A chop's rows are slices
  // and a kit's are drums, neither a note, so nothing is named above the squares - and without the
  // name the squares start where the matrix does, the two sides lined up.
  const int cellWidth = kSquare;
  const int cellHeight = kSquare;
  // Centred on the same band as the matrix, so no edge of one side hangs below the other.
  const int gridTop = kit ? kContentY + (kContentHeight - kStepBand) / 2 : kContentY + kNameHeight;
  const std::string chosenName = picked < 0 ? std::string("PADS: A NOTE") : drums ? std::string(kDrumNames[controller.DrumLane()]) : NoteName(picked);
  if (!kit) g.TextFit(left, kContentY + 1, kContentRight - left, chosenName, picked < 0 ? kFaint : kInk, picked < 0 ? Font::Micro : Font::Small);
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const Rect cell{left + static_cast<int>(index % 4) * (cellWidth + kGap), gridTop + static_cast<int>(index / 4) * (cellHeight + kGap), cellWidth, cellHeight};
    const std::size_t step = first + index;
    if (step >= length)
    {
      g.Pixel(cell.x + cell.width / 2, cell.y + cell.height / 2, kShade);
      continue;
    }
    const auto& info = seq.steps[index];
    std::uint8_t velocity = 0;
    std::uint8_t late = 0;
    std::uint8_t roll = 1;
    for (std::size_t slot = 0; slot < counts[index] && picked >= 0; ++slot)
      if (notes[index][slot].note == picked && velocity == 0)
      {
        velocity = std::max<std::uint8_t>(notes[index][slot].velocity, 1);
        late = notes[index][slot].microstep;
        roll = notes[index][slot].roll;
      }
    const bool marked = info.held;  // selected: inverse
    const float flash = info.playing ? fx::Decay(now, animator.StepPlayedAt(index), kStepFlashMs) : 0.0f;
    if (kit)
    {
      // A chop's square carries the two readings its matrix gives up: how hard, by how tall the bar
      // stands on the floor of the square, and ROLL, by that bar cut into the two, three or four
      // repeats the slice fires on. Where in the step a hit falls is left out - the cuts already
      // crowd it, and TIME has the hardware's own page. The square itself is a light gray key, so an
      // empty step still reads as one of the sixteen.
      const Rect inner{cell.x + 1, cell.y + 1, cell.width - 2, cell.height - 2};
      g.Fill(cell, kFaint);
      if (velocity != 0)
      {
        // The bar keeps the track's color, selected or not: how hard is the reading it carries, and
        // an inverse square would bury it.
        const Rect bar = VelocityBar(inner, velocity);
        g.Fill(bar, trackColor);
        for (int part = 1; part < roll; ++part)
        {
          // Three in a bar this narrow land as 7 - 6 - 6: the first cut a pixel left makes the
          // three parts the same width, the outer two matching.
          const int x = bar.x + bar.width * part / roll - (roll == 3 && part == 1 ? 1 : 0);
          if (x <= bar.x || x >= bar.x + bar.width) continue;
          g.Fill(Rect{x, bar.y, 1, bar.height}, kBlack);
        }
      }
      else if (info.hasEvents) g.Fill(Rect{inner.x, inner.y + inner.height - 2, inner.width, 2}, kDim);
      // The frame is the whole of the selection here: white for the step being edited, green while
      // it plays, the key's own gray for the rest of the grid.
      if (marked || info.playing) g.Stroke(cell, marked ? kInk : kPlay);
      continue;
    }
    // A hit fills the square whole — whether there is one is the GRID's question, how hard it was is
    // the step machine's and the piano roll's. ROLL reads as dots in the corner instead of cuts.
    if (marked) g.Fill(cell, kInk);
    else if (velocity != 0) g.Fill(cell, trackColor);
    else if (info.hasEvents) g.Fill(Rect{cell.x, cell.y + cell.height - 2, cell.width, 2}, kDim);
    if (info.playing && !marked)
    {
      g.Dither(Rect{cell.x + 1, cell.y + 1, cell.width - 2, cell.height - 2}, kPlay, std::max(3, static_cast<int>(flash * 16.0f)));
      g.Stroke(cell, kPlay);
    }
    else if (!marked && velocity == 0) g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, index % 4 == 0 ? kDim : kFaint, 1);
    if (velocity != 0) DrawRollDots(g, cell, roll);
    // A hit off the beat: a tick along the bottom, where in the step it falls.
    if (velocity != 0 && late != 0)
      g.Fill(Rect{cell.x + (cell.width - 4) * late / (synth::kSequencerMicrostepCount - 1), cell.y + cell.height - 3, 4, 2}, marked ? kBlack : kInk);
    // The step number reads on black or on the bar itself, whichever it happens to sit on.
    g.Text(cell.x + 2, cell.y + 2, Pad2(step + 1),
           marked || velocity != 0 ? kBlack : (index % 4 == 0 ? kDim : kFaint), Font::Micro);
  }
}

void SequenceScene(Gfx& g, const Controller& controller, Animator& animator, std::uint8_t page)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = controller.HostView().Sequencer();
  const auto seq = sequencer.BuildUiState(state.track);
  const auto& track = seq.tracks[state.track];
  switch (page)
  {
    case 0:
    {
      // ROLL or STEP (the machine), drum and melodic tracks alike.
      if (controller.View() == SeqView::Roll) DrawPianoRoll(g, controller, animator, seq);
      else DrawStepMachine(g, controller, animator, seq);
      if (!track.enabled)
      {
        const Rect band{kContentX, kContentY + kContentHeight / 2 - 6, kContentWidth, 12};
        g.Fill(band, kBlack);
        g.TextAligned(band, "SEQUENCE OFF - SEQ > TRACK", (now / 500) % 2 ? kDim : kFaint, Font::Micro, Align::Center);
      }
      break;
    }
    case 1:
      for (std::size_t index = 0; index < kGridCount; ++index)
      {
        const Rect cell = Cell(index / kGridColumns, index % kGridColumns);
        if (index > track.patternCount)
        {
          g.Pixel(cell.x + cell.width / 2, cell.y + cell.height / 2, kShade);
          continue;
        }
        if (index == track.patternCount)
        {
          // The "new" slot breathes to invite a press.
          const float breath = 0.5f + 0.5f * std::sin(static_cast<float>(now) / 380.0f);
          g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
          g.Text(cell.x + 3, cell.y + 2, "NEW", breath > 0.5f ? kDim : kFaint);
          g.DrawGlyph(cell.x + cell.width - 9, cell.y + 12, Glyph::Plus, breath > 0.5f ? kInk : kDim);
          GridRipple(g, animator, index, kTab);
          continue;
        }
        const bool active = index == track.activePattern;
        const bool queued = index == track.queuedPattern;
        const bool content = seq.patternHasEvents[index];
        Color ink = content ? kInk : kDim;
        if (active)
        {
          g.Fill(cell, kInk);
          ink = kBlack;
          const float landed = fx::Decay(now, animator.PatternChangedAt(state.track), 400);
          if (landed > 0.0f) fx::Ripple(g, cell, 1.0f - landed, kInk);
        }
        else if (content) g.Fill(Rect{cell.x, cell.y + cell.height - 2, cell.width, 2}, kDim);
        else g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
        if (queued)
        {
          // Queued: marching ants, plus a bar counting down to the switch at the loop end.
          fx::MarchingAnts(g, Rect{cell.x - 1, cell.y - 1, cell.width + 2, cell.height + 2}, kPlay, now);
          const float remaining = 1.0f - seq.patternProgress;
          g.Fill(Rect{cell.x + 2, cell.y + cell.height - 4, static_cast<int>(remaining * static_cast<float>(cell.width - 4)), 2}, kPlay);
        }
        char label[8];
        std::snprintf(label, sizeof(label), "P%02u", static_cast<unsigned>(index + 1));
        g.Text(cell.x + 3, cell.y + 2, label, ink, Font::Small);
        if (active && seq.playing)
        {
          const float beat = fx::Decay(now, animator.BeatAt(), 160);
          g.DrawGlyph(cell.x + cell.width - 8 + static_cast<int>(beat * 2.0f), cell.y + 3, Glyph::Play, kBlack);
        }
        if (queued) g.Text(cell.x + 3, cell.y + 14, "NEXT", active ? kBlack : kPlay);
        else if (active) g.Text(cell.x + 3, cell.y + 14, std::to_string(track.populatedStepCount) + " STEPS", kBlack);
        GridRipple(g, animator, index, active ? kInk : kTab);
      }
      break;
    case 2:
    default:
    {
      char steps[8], swing[8], bpm[8];
      std::snprintf(steps, sizeof(steps), "%u", static_cast<unsigned>(track.stepCount));
      std::snprintf(swing, sizeof(swing), "%u%%", static_cast<unsigned>(seq.swing));
      std::snprintf(bpm, sizeof(bpm), "%u", static_cast<unsigned>(seq.bpm));
      const std::array<const char*, 4> labels{{"SEQ", "STEPS", "SWING", "BPM"}};
      const std::array<std::string, 4> values{{track.enabled ? "ON" : "OFF", steps, swing, bpm}};
      const std::array<float, 4> targets{{-1.0f, static_cast<float>(track.stepCount - 1) / static_cast<float>(synth::kSequencerMaxSteps - 1),
                                          static_cast<float>(seq.swing - 50) / 25.0f, static_cast<float>(seq.bpm - 30) / 270.0f}};
      for (std::size_t index = 0; index < 4; ++index)
      {
        const float fraction = targets[index] < 0.0f ? -1.0f : animator.Smooth(Animator::Key(labels[index], 911u), targets[index]);
        widgets::ParamCell(g, Cell(0, index), labels[index], values[index], fraction,
                           index == state.seqTrackFocus ? CellStyle::Focused : CellStyle::Normal, 0.0f, now);
        GridRipple(g, animator, index, kInk);
      }
      const Rect table = CellSpan(1, 0, 3, 4);
      const Host& host = controller.HostView();
      for (std::size_t row = 0; row < kTrackCount; ++row)
      {
        const int y = table.y + 4 + static_cast<int>(row) * 17;
        const auto& summary = seq.tracks[row];
        const bool currentTrack = row == state.track;
        const Color ink = currentTrack ? kInk : kDim;
        if (currentTrack) g.Fill(Rect{table.x - 2, y - 1, 15, 11}, TrackColor(row));
        g.Text(table.x, y, "T" + std::to_string(row + 1), currentTrack ? kBlack : TrackColor(row), Font::Small);
        g.Text(table.x + 22, y + 1, EngineShortName(host.EngineKind(row)), ink);
        char pattern[16];
        std::snprintf(pattern, sizeof(pattern), "P%02u/%02u", static_cast<unsigned>(summary.activePattern + 1), static_cast<unsigned>(summary.patternCount));
        g.Text(table.x + 74, y + 1, pattern, ink);
        // Step ruler: one tick per step, the playhead in green.
        for (std::size_t step = 0; step < kGridCount; ++step)
        {
          const int x = table.x + 120 + static_cast<int>(step) * 5;
          // A long pattern is cut in as many slices as there are ticks.
          const bool inRange = step < summary.stepCount;
          const bool head = seq.playing && inRange && step == (summary.stepCount <= kGridCount ? summary.currentStep : summary.currentStep * kGridCount / summary.stepCount);
          const Color color = inRange ? (currentTrack ? kDim : kFaint) : kShade;
          g.Fill(Rect{x, y + 2, 3, 5}, head ? kPlay : color);
        }
        if (!summary.enabled) g.Text(table.x + table.width - Gfx::Measure("OFF"), y + 1, "OFF", kRec);
        const float activity = animator.MeterPeak(row);
        if (activity > 0.02f) g.Dither(Rect{table.x - 5, y + 1, 2, 8}, kPlay, static_cast<int>(activity * 16.0f));
      }
      break;
    }
  }
}

// SETUP SYSTEM: a terminal-style status report that types itself out (on every visit).
void DrawSystemInfo(Gfx& g, const Controller& controller, Animator& animator, const Rect& panel)
{
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const SystemInfo info = host.QuerySystem();
  int y = panel.y + 4;
  int line = 0;
  const float typed = fx::Clamp01(static_cast<float>(now - animator.CurrentTransition().start) / 600.0f);
  auto row = [&](const char* label, const std::string& value, Color color) {
    const float reveal = fx::Clamp01(typed * 5.0f - static_cast<float>(line));
    g.Text(panel.x, y, label, kDim);
    fx::TypeText(g, panel.x + 60, y, value, color, Font::Micro, reveal);
    y += 11;
    ++line;
  };
  row("VERSION", cardenza::build::kVersion, kInk);
  row("SD CARD", info.storageReady ? "READY" : "NOT FOUND", info.storageReady ? kPlay : kRec);
  // Internal RAM, and PSRAM beside it when there is some (one row: a fifth would run into the tabs).
  std::string memory = "N/A";
  if (info.freeHeap != 0 && info.totalPsram != 0)
    memory = std::to_string(info.freeHeap / 1024) + "K MIN " + std::to_string(info.minimumFreeHeap / 1024) + "K  PSRAM " +
             std::to_string(info.freePsram / 1024) + "K";
  else if (info.freeHeap != 0)
    memory = std::to_string(info.freeHeap / 1024) + "K FREE  MIN " + std::to_string(info.minimumFreeHeap / 1024) + "K";
  row("MEMORY", memory, kInk);
  // The Git hash (+ when built with local changes) and the UTC time of the build, month to minute.
  const std::string built = cardenza::build::kTimeUtc;
  row("BUILD", std::string(cardenza::build::kHash) + (cardenza::build::kDirty ? "+ " : " ") + built.substr(5, 11), kDim);
}

void ProjectScene(Gfx& g, const Controller& controller, Animator& animator, std::uint8_t page)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto seq = host.Sequencer().BuildUiState(state.track);
  switch (page)
  {
    case 0:
    {
      const std::array<const char*, 3> actions{{"SAVE", "LOAD", "NEW"}};
      for (std::size_t index = 0; index < actions.size(); ++index)
      {
        widgets::ActionCell(g, Cell(0, index), actions[index], state.gridDown[index], kDim);
        GridRipple(g, animator, index, kInk);
      }
      char bpm[8], swing[8];
      std::snprintf(bpm, sizeof(bpm), "%u", static_cast<unsigned>(seq.bpm));
      std::snprintf(swing, sizeof(swing), "%u%%", static_cast<unsigned>(seq.swing));
      const float beat = seq.playing ? fx::Decay(now, animator.BeatAt(), 150) : 0.0f;
      widgets::ParamCell(g, Cell(1, 0), "BPM", bpm, animator.Smooth(Animator::Key("proj-bpm"), static_cast<float>(seq.bpm - 30) / 270.0f),
                         state.projectFocus == 4 ? CellStyle::Focused : CellStyle::Normal, beat * 0.6f, now);
      widgets::ParamCell(g, Cell(1, 1), "SWING", swing, animator.Smooth(Animator::Key("proj-swing"), static_cast<float>(seq.swing - 50) / 25.0f),
                         state.projectFocus == 5 ? CellStyle::Focused : CellStyle::Normal, 0.0f, now);
      widgets::ActionCell(g, Cell(1, 2), "TAP", state.gridDown[6], kDim);
      // PLAY: a switch, LOOP (each track loops its pattern) or SONG (the arrangement plays).
      const bool song = host.Sequencer().SongMode();
      widgets::ParamCell(g, Cell(1, 3), "PLAY", song ? "SONG" : "LOOP", song ? 1.0f : 0.0f, CellStyle::Normal, 0.0f, now);
      for (std::size_t index = 4; index < 8; ++index) GridRipple(g, animator, index, kInk);
      const Rect info = CellSpan(2, 0, 2, 4);
      const std::string name = state.projectName.empty() ? std::string("UNTITLED") : state.projectName;
      const int x = fx::DecodeText(g, info.x, info.y + 2, name, state.projectName.empty() ? kDim : kInk, Font::Large,
                                   fx::Progress(now, animator.ProjectChangedAt(), 500), animator.ProjectChangedAt() + now / 60, info.width - 60);
      if (state.dirty) g.Text(x + 6, info.y + 6, "UNSAVED", (now / 700) % 2 ? kFn : kDim);
      else if (!state.projectName.empty()) g.Text(x + 6, info.y + 6, "SAVED", kDim);
      for (std::size_t track = 0; track < kTrackCount; ++track)
      {
        const int column = static_cast<int>(track % 2);
        const int row = static_cast<int>(track / 2);
        const int tx = info.x + column * (info.width / 2);
        const int ty = info.y + 24 + row * 12;
        const float activity = animator.MeterPeak(track);
        g.Text(tx, ty, "T" + std::to_string(track + 1), activity > 0.3f ? kInk : TrackColor(track));
        const auto* engine = host.Engine(track);
        g.Text(tx + 16, ty, EngineCode(host.EngineKind(track)), kDim);
        if (engine != nullptr) g.TextFit(tx + 40, ty, info.width / 2 - 44, engine->CurrentPresetName(), kInk);
      }
      break;
    }
    case 1:
    {
      const Rect list{kContentX, kContentY, kContentWidth - 8, kContentHeight};
      if (state.projects.empty())
      {
        g.TextAligned(list, "NO PROJECTS ON THE CARD", (now / 600) % 2 ? kDim : kFaint, Font::Micro, Align::Center);
        break;
      }
      const std::size_t visible = 10;
      const std::size_t first = state.fileCursor >= visible ? state.fileCursor - visible + 1 : 0;
      // The highlight bar glides between rows.
      const float barY = animator.Smooth(Animator::Key("files-cursor"), static_cast<float>(list.y + 1 + static_cast<int>(state.fileCursor - first) * 10), 45.0f);
      g.Fill(Rect{list.x, static_cast<int>(std::lround(barY)), list.width, 9}, kInk);
      for (std::size_t row = 0; row < visible && first + row < state.projects.size(); ++row)
      {
        const std::size_t index = first + row;
        const int y = list.y + 2 + static_cast<int>(row) * 10;
        const bool covered = std::abs(static_cast<float>(y - 1) - barY) < 5.0f;
        const bool current = state.projects[index] == state.projectName;
        g.Text(list.x + 2, y, current ? ">" : " ", covered ? kBlack : kTab);
        g.TextFit(list.x + 10, y, list.width - 12, state.projects[index], covered ? kBlack : kInk);
      }
      widgets::ScrollIndicator(g, Rect{kContentRight - 2, kContentY + 1, 1, kContentHeight - 2}, first, visible, state.projects.size());
      break;
    }
  }
}

// TAPE: the song as a strip of tape. Columns are song rows (time runs left to right), rows are tracks,
// exactly as on the GRID keys. The playing row carries a moving tape head.
void TapeScene(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = host.Sequencer();
  const auto seq = sequencer.BuildUiState(state.track);
  const std::size_t length = seq.songLength;
  for (std::size_t column = 0; column < kGridColumns; ++column)
  {
    const std::size_t row = state.songScroll + column;
    const Rect top = Cell(0, column);
    const Rect bottom = Cell(kRows - 1, column);
    const Rect span{top.x, top.y, top.width, bottom.y + bottom.height - top.y};
    if (row > length || row >= synth::kSongMaxRows)
    {
      g.Pixel(span.x + span.width / 2, span.y + span.height / 2, kShade);
      continue;
    }
    if (row == length)
    {
      // The empty column after the song: press to append a row.
      const float breath = 0.5f + 0.5f * std::sin(static_cast<float>(now) / 380.0f);
      g.DottedVLine(span.x, span.y, span.height, kFaint, 2);
      g.Text(span.x + 4, span.y + 2, "ROW", breath > 0.5f ? kDim : kFaint);
      g.DrawGlyph(span.x + span.width / 2 - 2, span.y + span.height / 2 - 3, Glyph::Plus, breath > 0.5f ? kInk : kDim);
      for (std::size_t track = 0; track < kRows; ++track) GridRipple(g, animator, track * kGridColumns + column, kTab);
      continue;
    }
    const synth::SongRow songRow = sequencer.SongRowAt(row);
    const bool playingRow = seq.songPlaying && row == seq.songRow;
    const bool cued = seq.songCueRow == row;
    if (playingRow)
    {
      // Tape head: a green line sweeps through the row while it plays.
      g.Dither(span, kPlay, 2);
      const int head = span.x + static_cast<int>(seq.songRowProgress * static_cast<float>(span.width - 1));
      g.VLine(head, span.y, span.height, kPlay);
    }
    if (cued) fx::MarchingAnts(g, Rect{span.x - 2, span.y - 2, span.width + 4, span.height + 4}, kFn, now);
    for (std::size_t track = 0; track < kRows; ++track)
    {
      const Rect cell = Cell(track, column);
      const int pattern = songRow.patterns[track];
      const bool cursor = row == state.songRow && track == state.songTrack;
      Color ink = pattern < 0 ? kFaint : kInk;
      if (cursor)
      {
        g.Fill(cell, kInk);
        ink = kBlack;
      }
      else if (pattern >= 0)
        g.Fill(Rect{cell.x, cell.y + cell.height - 2, cell.width, 2}, playingRow ? kPlay : TrackColor(track));
      else g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
      if (track == 0)
      {
        // Row number and length (bars) ride in the first track's cell.
        char label[16];
        std::snprintf(label, sizeof(label), "%02u", static_cast<unsigned>(row + 1));
        g.Text(cell.x + 3, cell.y + 2, label, cursor ? kBlack : (playingRow ? kPlay : kDim));
        std::snprintf(label, sizeof(label), "x%u", static_cast<unsigned>(songRow.repeats));
        const int barsX = cell.x + cell.width - Gfx::Measure(label) - 2;
        // While the cursor edits the row length (BARS lane), its bar count is the highlighted value.
        if (state.songLane == 1 && row == state.songRow)
        {
          g.Fill(Rect{barsX - 2, cell.y + 1, Gfx::Measure(label) + 4, 9}, kFn);
          g.Text(barsX, cell.y + 2, label, kBlack);
        }
        else g.Text(barsX, cell.y + 2, label, cursor ? kBlack : kFn);
      }
      else g.Text(cell.x + 3, cell.y + 2, "T" + std::to_string(track + 1), cursor ? kBlack : TrackColor(track));
      char name[8];
      if (pattern < 0) std::snprintf(name, sizeof(name), "--");
      else std::snprintf(name, sizeof(name), "P%02d", pattern + 1);
      g.Text(cell.x + 3, cell.y + 12, name, ink, Font::Small);
      if (cursor) fx::MarchingAnts(g, Rect{cell.x - 1, cell.y - 1, cell.width + 2, cell.height + 2}, kInk, now);
      GridRipple(g, animator, track * kGridColumns + column, cursor ? kInk : kTab);
    }
  }
  if (length == 0)
  {
    const Rect hint{kContentX + 60, kContentY + kContentHeight / 2 - 10, kContentWidth - 60, 20};
    g.TextAligned(hint, "TAP THE + COLUMN ON THE GRID", (now / 600) % 2 ? kDim : kFaint, Font::Micro, Align::Center);
  }
  // Mode badge, bottom right of the tape.
  const char* mode = seq.songMode ? "SONG" : "LOOP";
  const Rect badge{kContentRight - Gfx::Measure(mode) - 4, kContentBottom - 9, Gfx::Measure(mode) + 4, 9};
  g.Fill(badge, seq.songMode ? kFn : kFaint);
  g.Text(badge.x + 2, badge.y + 1, mode, kBlack);
}

void MixScene(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto seq = host.Sequencer().BuildUiState(state.track);
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    const Rect top = TrackCell(0, track);
    const Rect bottom = TrackCell(3, track);
    const bool focused = state.mixFocus == track;
    const bool audible = host.TrackAudible(track);
    const Rect fader{top.x + 1, top.y + 1, 9, bottom.y + bottom.height - top.y - 2};
    const float level = animator.Smooth(Animator::Key("mix-level", static_cast<std::uint32_t>(track)), static_cast<float>(host.TrackLevel(track)) / 100.0f);
    widgets::Fader(g, fader, level, audible ? kInk : kFaint, focused);
    if (focused) fx::MarchingAnts(g, Rect{fader.x - 1, fader.y - 1, fader.width + 2, fader.height + 2}, TrackColor(track), now);
    // Activity meter with a falling peak marker.
    const float activity = animator.Smooth(Animator::Key("mix-act", static_cast<std::uint32_t>(track)), host.TrackActivity(track), 35.0f);
    const int meter = static_cast<int>(activity * static_cast<float>(fader.height));
    const Rect meterRect{fader.x + fader.width + 2, fader.y + fader.height - meter, 2, meter};
    // The bar is drawn whenever the track is sounding: a muted or un-soloed channel still shows what
    // it would put out, in a colour that reads against the panel.
    g.Fill(meterRect, meter > 0 ? kPlay : kFaint);
    const int peakY = fader.y + fader.height - static_cast<int>(animator.MeterPeak(track) * static_cast<float>(fader.height));
    if (animator.MeterPeak(track) > 0.02f) g.HLine(fader.x + fader.width + 1, peakY, 4, audible ? kInk : kFaint);

    const int x = top.x + 17;
    const int width = top.x + top.width - x;
    const std::string label = "T" + std::to_string(track + 1);
    if (focused)
    {
      g.Fill(Rect{x - 2, top.y, width + 2, 10}, TrackColor(track));
      g.Text(x, top.y + 2, label, kBlack);
    }
    else g.Text(x, top.y + 2, label, TrackColor(track));
    if (width >= 30) g.TextFit(x + 15, top.y + 2, width - 15, EngineCode(host.EngineKind(track)), focused ? kBlack : kDim);
    g.Text(x, top.y + 13, std::to_string(host.TrackLevel(track)), audible ? kInk : kDim, Font::Small);

    const auto badge = [&](std::size_t row) {
      const Rect cell = TrackCell(row, track);
      return Rect{x - 2, cell.y + 4, cell.x + cell.width - (x - 2), 13};
    };
    const std::array<bool, 3> states{{host.TrackMuted(track), host.TrackSoloed(track), seq.tracks[track].enabled}};
    const std::array<const char*, 3> names{{"MUTE", "SOLO", "SEQ"}};
    const std::array<Color, 3> colors{{kRec, kFn, kDim}};
    for (std::size_t kind = 0; kind < 3; ++kind)
    {
      // Toggling pops the badge and throws a ripple.
      const float pop = fx::Decay(now, animator.MixToggledAt(track, kind), 300);
      Rect rect = badge(kind + 1);
      if (pop > 0.0f)
      {
        const int grow = static_cast<int>(fx::EaseOutBack(1.0f - pop) * 2.0f * pop);
        rect = Rect{rect.x - grow, rect.y - grow, rect.width + grow * 2, rect.height + grow * 2};
        fx::Ripple(g, rect, 1.0f - pop, colors[kind] == kDim ? kInk : colors[kind]);
      }
      widgets::Badge(g, rect, names[kind], states[kind], colors[kind]);
    }
    for (std::size_t row = 0; row < kRows; ++row) GridRipple(g, animator, row * kGridColumns + track, kInk);
  }
}

// SESSION: the clip grid. Columns are the tracks (in their colors), rows are pattern slots. Each
// cell shows its pattern's steps as ticks; the playing pattern of a track carries a moving playhead
// and a green wash, a queued one crawling yellow ants, the next free slot a "+" to create it.
void SessionScene(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = host.Sequencer();
  const auto seq = sequencer.BuildUiState(state.track);
  const std::size_t base = controller.GridTrackBase();
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const std::size_t track = base + index % kGridColumns;
    const std::size_t pattern = state.sessionScroll + index / kGridColumns;
    const Rect cell = Cell(index / kGridColumns, index % kGridColumns);
    const auto& summary = seq.tracks[track];
    const Color color = TrackColor(track);
    g.Fill(Rect{cell.x - 2, cell.y, 1, cell.height}, color);
    if (pattern >= summary.patternCount)
    {
      if (pattern == summary.patternCount && pattern < synth::kSequencerMaxPatternCount)
      {
        g.DottedHLine(cell.x, cell.y, cell.width, kFaint);
        g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
        g.TextAligned(cell, "+", kDim, Font::Small, Align::Center);
      }
      else g.Pixel(cell.x + cell.width / 2, cell.y + cell.height / 2, kShade);
      GridRipple(g, animator, index, kInk);
      continue;
    }
    const bool active = pattern == summary.activePattern;
    const bool playing = active && seq.playing && !summary.songSilent;
    const bool queued = pattern == summary.queuedPattern;
    const bool audible = summary.enabled && !summary.muted && host.TrackAudible(track);
    if (playing) g.Dither(cell, kPlay, 3);
    if (active) g.Stroke(cell, color);
    const std::uint16_t mask = sequencer.PatternStepMask(track, pattern);
    char name[12];
    std::snprintf(name, sizeof(name), "P%02u", static_cast<unsigned>(pattern + 1));
    g.Text(cell.x + 3, cell.y + 3, name, active ? color : (mask != 0 ? kInk : kDim));
    if (playing) g.DrawGlyph(cell.x + cell.width - 9, cell.y + 3, Glyph::Play, kPlay);
    else if (queued && (now / 250) % 2 == 0) g.Text(cell.x + cell.width - Gfx::Measure("NXT") - 3, cell.y + 3, "NXT", kFn);
    // Steps as ticks: filled where the pattern has notes, the playhead in white.
    const int left = cell.x + 3;
    const int span = cell.width - 6;
    for (std::size_t step = 0; step < summary.stepCount && step < synth::kSequencerStepCount; ++step)
    {
      const int x = left + static_cast<int>(step) * span / static_cast<int>(synth::kSequencerStepCount);
      const bool on = ((mask >> step) & 1u) != 0;
      const bool head = playing && step == (summary.stepCount <= synth::kSequencerStepCount ? summary.currentStep : summary.currentStep * synth::kSequencerStepCount / summary.stepCount);
      if (head) g.Fill(Rect{x, cell.y + 13, 2, 7}, kInk);
      else if (on) g.Fill(Rect{x, cell.y + 15, 2, 5}, audible ? color : kDim);
      else g.Pixel(x, cell.y + 19, kFaint);
    }
    if (queued) fx::MarchingAnts(g, Rect{cell.x - 1, cell.y - 1, cell.width + 2, cell.height + 2}, kFn, now);
    GridRipple(g, animator, index, kInk);
  }
  // The cursor glides like every focus frame.
  if (state.sessionPattern >= state.sessionScroll && state.sessionPattern < state.sessionScroll + kRows)
  {
    const std::size_t focusColumn = state.sessionTrack >= base ? state.sessionTrack - base : 0;
    const Rect target = Cell(state.sessionPattern - state.sessionScroll, focusColumn);
    const std::uint32_t key = Animator::Key("session-focus");
    const int x = static_cast<int>(std::lround(animator.Smooth(key, static_cast<float>(target.x), 50.0f)));
    const int y = static_cast<int>(std::lround(animator.Smooth(key + 1, static_cast<float>(target.y), 50.0f)));
    fx::MarchingAnts(g, Rect{x - 2, y - 2, target.width + 4, target.height + 4}, kInk, now);
  }
  widgets::ScrollIndicator(g, Rect{kContentRight, kContentY + 1, 1, kContentHeight - 2}, state.sessionScroll, kRows, synth::kSequencerMaxPatternCount);
}

// SENDS: one column per track (like MIX), one row per effect: filter, chorus, delay send, reverb
// send. A cell glows at its right edge while its track sounds into an open send.
void SendsScene(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  for (std::size_t row = 0; row < kSendIds.size(); ++row)
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    const std::size_t index = row * kTrackCount + track;
    const Rect cell = TrackCell(row, track);
    const auto* engine = host.Engine(track);
    const std::string label = "T" + std::to_string(track + 1) + kSendTags[row];
    synth::SynthParameterInfo info;
    if (engine == nullptr || !engine->GetParameterInfo(kSendIds[row], info))
    {
      widgets::ParamCell(g, cell, label, "--", -1.0f, CellStyle::Disabled);
      continue;
    }
    const bool focused = state.sendsFocus == index;
    const bool off = info.value == info.defaultValue;
    const CellStyle style = focused ? CellStyle::Focused : off ? CellStyle::Disabled : CellStyle::Normal;
    g.Fill(Rect{cell.x - 2, cell.y, 1, cell.height}, TrackColor(track));
    const std::string text = engine->DescribeParameterValue(kSendIds[row]);
    const std::uint32_t key = Animator::Key(kSendIds[row], static_cast<std::uint32_t>(track) * 53u + 5u);
    if (row == 0)
    {
      // The DJ filter is bipolar: low-pass to the left of the centre tick, high-pass to the right.
      widgets::ParamCell(g, cell, label, text, -1.0f, style, 0.0f, now, false);
      const float value = animator.Smooth(key, static_cast<float>(info.value - synth::fx::kFilterCenter) / 64.0f, 60.0f);
      widgets::BipolarBar(g, Rect{cell.x + 3, cell.y + cell.height - 2, cell.width - 5, 2}, value, kInk, kFaint);
    }
    else
    {
      const float fraction = animator.Smooth(key, static_cast<float>(info.value - info.minValue) / static_cast<float>(info.maxValue - info.minValue), 60.0f);
      widgets::ParamCell(g, cell, label, text, fraction, style, 0.0f, now, false);
    }
    const float activity = host.TrackAudible(track) ? host.TrackActivity(track) : 0.0f;
    if (!off && activity > 0.02f) g.Dither(Rect{cell.x + cell.width - 3, cell.y + 1, 2, cell.height - 2}, kTab, static_cast<int>(4.0f + activity * 12.0f));
    GridRipple(g, animator, index, kInk);
  }
  const Rect focus = TrackCell(std::min<std::size_t>(state.sendsFocus / kTrackCount, kSendIds.size() - 1),
                               state.sendsFocus % kTrackCount);
  const std::uint32_t key = Animator::Key("sends-focus");
  const int x = static_cast<int>(std::lround(animator.Smooth(key, static_cast<float>(focus.x), 50.0f)));
  const int y = static_cast<int>(std::lround(animator.Smooth(key + 1, static_cast<float>(focus.y), 50.0f)));
  fx::MarchingAnts(g, Rect{x - 1, y - 1, focus.width + 2, focus.height + 2}, kInk, now);
}

std::string MasterFxText(const Host& host, std::size_t param)
{
  using synth::fx::MasterParam;
  const int value = host.MasterFx(param);
  switch (static_cast<MasterParam>(param))
  {
    case MasterParam::DelayTime: return synth::fx::kDelayDivisions[static_cast<std::size_t>(std::clamp(value, 0, 11))];
    case MasterParam::EqLow:
    case MasterParam::EqHigh:
    {
      const int db = (value - 64) * 12 / 64;
      return (db > 0 ? "+" : "") + std::to_string(db) + "DB";
    }
    case MasterParam::Compressor:
    case MasterParam::Tape:
      return value == 0 ? "OFF" : std::to_string(value);
    default: return std::to_string(value);
  }
}

// FX: the master bus. Row 0 delay (time synced to the tempo), row 1 reverb, row 2 the master
// compressor, EQ and tape saturation; row 3 draws what the two buses are doing.
void FxScene(Gfx& g, const Controller& controller, Animator& animator)
{
  using synth::fx::MasterParam;
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  static constexpr std::array<const char*, synth::fx::kMasterParamCount> kLabels{{
      "DELAY", "FDBK", "TONE", "RETURN", "REVERB", "DAMP", "LOCUT", "RETURN", "COMP", "LOW", "HIGH", "TAPE"}};
  for (std::size_t param = 0; param < synth::fx::kMasterParamCount; ++param)
  {
    const auto& info = synth::fx::kMasterParams[param];
    const Rect cell = Cell(param / kGridColumns, param % kGridColumns);
    const int value = host.MasterFx(param);
    const bool focused = state.fxFocus == param;
    const bool eq = param == static_cast<std::size_t>(MasterParam::EqLow) || param == static_cast<std::size_t>(MasterParam::EqHigh);
    const std::uint32_t key = Animator::Key("master-fx", static_cast<std::uint32_t>(param));
    if (eq)
    {
      widgets::ParamCell(g, cell, kLabels[param], MasterFxText(host, param), -1.0f, focused ? CellStyle::Focused : CellStyle::Normal, 0.0f, now, false);
      widgets::BipolarBar(g, Rect{cell.x + 3, cell.y + cell.height - 2, cell.width - 5, 2},
                          animator.Smooth(key, static_cast<float>(value - 64) / 64.0f, 60.0f), kInk, kFaint);
    }
    else
    {
      const float fraction = animator.Smooth(key, static_cast<float>(value - info.minValue) / static_cast<float>(info.maxValue - info.minValue), 60.0f);
      widgets::ParamCell(g, cell, kLabels[param], MasterFxText(host, param), fraction, focused ? CellStyle::Focused : CellStyle::Normal, 0.0f, now, false);
    }
    GridRipple(g, animator, param, kInk);
  }
  const Rect focus = Cell(state.fxFocus / kGridColumns, state.fxFocus % kGridColumns);
  const std::uint32_t focusKey = Animator::Key("fx-focus");
  const int fx0 = static_cast<int>(std::lround(animator.Smooth(focusKey, static_cast<float>(focus.x), 50.0f)));
  const int fy0 = static_cast<int>(std::lround(animator.Smooth(focusKey + 1, static_cast<float>(focus.y), 50.0f)));
  fx::MarchingAnts(g, Rect{fx0 - 1, fy0 - 1, focus.width + 2, focus.height + 2}, kInk, now);

  // Delay: the echo train at the current tempo, each repeat quieter by the feedback.
  const Rect delayArea = CellSpan(3, 0, 1, 2);
  const auto seq = host.Sequencer().BuildUiState(state.track);
  const int division = std::clamp(host.MasterFx(static_cast<std::size_t>(MasterParam::DelayTime)), 0, 11);
  const float delayMs = synth::fx::kDelaySixteenths[static_cast<std::size_t>(division)] * 15000.0f / static_cast<float>(std::max<std::uint16_t>(seq.bpm, 30));
  const float feedback = static_cast<float>(host.MasterFx(static_cast<std::size_t>(MasterParam::DelayFeedback))) / 127.0f * 0.95f;
  const float delayReturn = host.FxReturnLevel(synth::fx::kBusDelay);
  char delayLabel[24];
  std::snprintf(delayLabel, sizeof(delayLabel), "%dMS", static_cast<int>(delayMs));
  g.Text(delayArea.x + 1, delayArea.y, delayLabel, kDim);
  const float spanMs = 2000.0f;
  float amplitude = 1.0f;
  for (int repeat = 0; repeat < 24 && amplitude > 0.04f; ++repeat)
  {
    const int x = delayArea.x + 1 + static_cast<int>(static_cast<float>(repeat) * delayMs / spanMs * static_cast<float>(delayArea.width - 2));
    if (x >= delayArea.x + delayArea.width) break;
    const int height = std::max(1, static_cast<int>(amplitude * static_cast<float>(delayArea.height - 9)));
    g.VLine(x, delayArea.y + delayArea.height - height, height, repeat == 0 ? kInk : kTab);
    amplitude *= feedback;
  }
  if (!host.FxBusAvailable(synth::fx::kBusDelay)) g.Text(delayArea.x + delayArea.width - Gfx::Measure("NO MEM"), delayArea.y, "NO MEM", kRec);
  else if (delayReturn > 0.01f) g.Dither(Rect{delayArea.x + delayArea.width - 4, delayArea.y, 3, delayArea.height}, kTab, static_cast<int>(4.0f + delayReturn * 12.0f));

  // Reverb: the decay curve for the current size, its top cut by the damping.
  const Rect reverbArea = CellSpan(3, 2, 1, 2);
  const float size = static_cast<float>(host.MasterFx(static_cast<std::size_t>(MasterParam::ReverbSize))) / 127.0f;
  const float seconds = 0.3f * std::pow(20.0f, size);
  const float damp = static_cast<float>(host.MasterFx(static_cast<std::size_t>(MasterParam::ReverbDamp))) / 127.0f;
  char reverbLabel[24];
  std::snprintf(reverbLabel, sizeof(reverbLabel), "%.1fS", static_cast<double>(std::min(seconds, 99.0f)));
  g.Text(reverbArea.x + 1, reverbArea.y, reverbLabel, kDim);
  const Rect curve{reverbArea.x + 1, reverbArea.y + 8, reverbArea.width - 6, reverbArea.height - 8};
  int previousY = curve.y;
  for (int column = 0; column < curve.width; ++column)
  {
    const float t = static_cast<float>(column) / static_cast<float>(curve.width) * 6.0f;  // 0..6 s
    const float level = std::max(0.0f, 1.0f - t / std::max(0.1f, seconds));  // -60 dB at the decay time
    const int y = curve.y + curve.height - 1 - static_cast<int>(level * (1.0f - 0.4f * damp) * static_cast<float>(curve.height - 1));
    g.Dither(Rect{curve.x + column, y, 1, curve.y + curve.height - y}, kTab, 4);
    if (column > 0) g.Line(curve.x + column - 1, previousY, curve.x + column, y, kInk);
    previousY = y;
  }
  const float reverbReturn = host.FxReturnLevel(synth::fx::kBusReverb);
  if (!host.FxBusAvailable(synth::fx::kBusReverb)) g.Text(reverbArea.x + reverbArea.width - Gfx::Measure("NO MEM"), reverbArea.y, "NO MEM", kRec);
  else if (reverbReturn > 0.01f) g.Dither(Rect{reverbArea.x + reverbArea.width - 4, reverbArea.y, 3, reverbArea.height}, kTab, static_cast<int>(4.0f + reverbReturn * 12.0f));
}

}  // namespace

void MenuContent(Gfx& g, const Controller& controller, Animator& animator, float t)
{
  // One tile per item, on the GRID key that runs it: the name, what it acts on (or its state), and
  // the FN shortcut in yellow at the top right. Row 3 is the same on every screen.
  const std::uint32_t now = animator.Now();
  const MenuItems menu = controller.Menu();
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const std::size_t row = index / kGridColumns;
    const std::size_t column = index % kGridColumns;
    const float tileT = fx::EaseOutBack(fx::Clamp01(t * 1.8f - static_cast<float>(row + column) * 0.09f));
    Rect tile = Cell(row, column);
    tile.y -= static_cast<int>((1.0f - tileT) * 8.0f);
    const MenuItem& item = menu[index];
    if (item.action == MenuAction::None)
    {
      g.Pixel(tile.x + tile.width / 2, tile.y + tile.height / 2, kShade);
      continue;
    }
    const bool usable = item.available;
    // The item just run: solid FN yellow while its key is held and a moment after, so the press is
    // seen on the tile it belongs to (the result is in the footer).
    const auto& press = controller.State().menuPress;
    const bool lit = usable && press.index == index && (press.down || (press.releasedAt != 0 && now - press.releasedAt < kMenuPressMs));
    g.Fill(tile, kBlack);
    if (lit)
    {
      g.Fill(tile, kFn);
      const Color ink = kBlack;
      int keyWidth = 0;
      if (item.key != nullptr)
      {
        keyWidth = Gfx::Measure(item.key) + 3;
        g.Text(tile.x + tile.width - keyWidth, tile.y + 2, item.key, ink);
      }
      g.TextFit(tile.x + 3, tile.y + 2, tile.width - 5 - keyWidth, item.label, ink);
      g.TextFit(tile.x + 3, tile.y + 13, tile.width - 5, item.detail, ink);
      continue;
    }
    if (item.on) g.Fill(tile, kInk);
    else if (usable)
    {
      g.Dither(tile, kFaint, 2);
      g.Stroke(tile, row == kRows - 1 ? kFaint : kDim);
    }
    else
    {
      g.DottedHLine(tile.x, tile.y, tile.width, kShade);
      g.DottedHLine(tile.x, tile.y + tile.height - 1, tile.width, kShade);
    }
    const Color ink = item.on ? kBlack : usable ? kInk : kFaint;
    int keyWidth = 0;
    if (item.key != nullptr)
    {
      keyWidth = Gfx::Measure(item.key) + 3;
      g.Text(tile.x + tile.width - keyWidth, tile.y + 2, item.key, item.on ? kBlack : usable ? kFn : kShade);
    }
    g.TextFit(tile.x + 3, tile.y + 2, tile.width - 5 - keyWidth, item.label, ink);
    g.TextFit(tile.x + 3, tile.y + 13, tile.width - 5, item.detail, item.on ? kBlack : usable ? kDim : kShade);
  }
}

void SceneContent(Gfx& g, const Controller& controller, Animator& animator, Scene scene, std::uint8_t page)
{
  switch (scene)
  {
    case Scene::Instrument: InstrumentScene(g, controller, animator, page); break;
    case Scene::Sequence: SequenceScene(g, controller, animator, page); break;
    case Scene::Tape:
      if (page == kTapeSession) SessionScene(g, controller, animator);
      else if (page == kTapeSong) TapeScene(g, controller, animator);
      else if (page == kTapeMix) MixScene(g, controller, animator);
      else if (page == kTapeSends) SendsScene(g, controller, animator);
      else FxScene(g, controller, animator);
      break;
    case Scene::Setup:
      if (page < kSetupKeys) ProjectScene(g, controller, animator, page);
      else
      {
        // KEYS, CHORD, ARP, SYSTEM: slot pages like INSTR.
        const std::size_t settings = std::min<std::size_t>(page - kSetupKeys, kSettingsPageCount - 1);
        const UiState& state = controller.State();
        DrawSlotPage(g, controller, animator, SettingsPage(settings), state.settingsFocus[settings], page == controller.CurrentPage(),
                     Animator::Key("settings-focus", static_cast<std::uint32_t>(settings)));
      }
      break;
  }
}

}  // namespace groove::render
