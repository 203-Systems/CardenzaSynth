#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

constexpr std::uint32_t kStampMs = 360;
constexpr std::uint32_t kRippleMs = 300;
constexpr std::uint32_t kTrailMs = 420;
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
  char text[4];
  std::snprintf(text, sizeof(text), "%02u", static_cast<unsigned>(value));
  return text;
}

void GridRipple(Gfx& g, Animator& animator, std::size_t index, Color color)
{
  const std::uint32_t pressed = animator.GridPressedAt(index);
  if (pressed == 0) return;
  fx::Ripple(g, Cell(index / kGridColumns, index % kGridColumns), fx::Progress(animator.Now(), pressed, kRippleMs), color);
}

void DrawPads(Gfx& g, const Controller& controller, Animator& animator, const Rect& area, const synth::ISynthEngine* engine)
{
  const int padWidth = (area.width - 9) / 4;
  const int padHeight = (area.height - 3) / 2;
  static constexpr std::array<const char*, 8> kIds{{"kick.level", "snare.level", "closed_hat.level", "open_hat.level",
                                                    "clap.level", "tom.level", "cowbell.level", "maraca.level"}};
  const std::uint32_t now = animator.Now();
  for (std::size_t pad = 0; pad < 8; ++pad)
  {
    const Rect rect{area.x + static_cast<int>(pad % 4) * (padWidth + 3), area.y + static_cast<int>(pad / 4) * (padHeight + 3), padWidth, padHeight};
    const bool held = controller.IsNoteHeld(kDrumNotes[pad]);
    const float hit = fx::Decay(now, animator.NoteHitAt(kDrumNotes[pad]), kAfterglowMs);
    const float level = Glide(animator, controller.State().track, engine, kIds[pad], 0.8f);
    // A hit kicks the pad up a few pixels and leaves a fading glow.
    const int kick = static_cast<int>(hit * hit * 3.0f);
    const Rect body{rect.x, rect.y - kick, rect.width, rect.height};
    if (held) g.Fill(body, kInk);
    else
    {
      const int filled = static_cast<int>(level * static_cast<float>(body.height - 2));
      g.Dither(Rect{body.x + 1, body.y + body.height - 1 - filled, body.width - 2, filled}, kFaint, 4);
      if (hit > 0.0f) g.Dither(body, kInk, static_cast<int>(hit * 12.0f));
      g.Stroke(body, hit > 0.0f ? kInk : kFaint);
    }
    g.Text(body.x + 3, body.y + 3, kDrumNames[pad], held ? kBlack : kInk);
    if (hit > 0.0f) fx::Ripple(g, body, 1.0f - hit, kInk);
  }
}

void DrawMiniPiano(Gfx& g, const Rect& rect, const Controller& controller, Animator& animator, int baseNote)
{
  // Two octaves: outlined white keys, dithered black keys; held notes light up and fade after release.
  static constexpr std::array<int, 7> kWhite{{0, 2, 4, 5, 7, 9, 11}};
  static constexpr std::array<int, 7> kBlackAfter{{1, 3, -1, 6, 8, 10, -1}};
  const std::uint32_t now = animator.Now();
  const int whiteCount = 14;
  const int keyWidth = rect.width / whiteCount;
  const int left = rect.x + (rect.width - keyWidth * whiteCount) / 2;
  for (int index = 0; index < whiteCount; ++index)
  {
    const int note = baseNote + (index / 7) * 12 + kWhite[static_cast<std::size_t>(index % 7)];
    const Rect key{left + index * keyWidth, rect.y, keyWidth - 1, rect.height};
    const float glow = fx::Decay(now, animator.NoteHitAt(note), kAfterglowMs);
    if (controller.IsNoteHeld(note)) g.Fill(key, kInk);
    else
    {
      if (glow > 0.0f) g.Dither(key, kInk, static_cast<int>(glow * 11.0f));
      g.Stroke(key, index % 7 == 0 ? kDim : kFaint);
    }
  }
  for (int index = 0; index < whiteCount; ++index)
  {
    const int black = kBlackAfter[static_cast<std::size_t>(index % 7)];
    if (black < 0) continue;
    const int note = baseNote + (index / 7) * 12 + black;
    const Rect key{left + index * keyWidth + keyWidth * 2 / 3, rect.y, keyWidth * 2 / 3, rect.height * 3 / 5};
    const bool held = controller.IsNoteHeld(note);
    const float glow = fx::Decay(now, animator.NoteHitAt(note), kAfterglowMs);
    g.Fill(key, held ? kTab : kBlack);
    if (!held)
    {
      g.Dither(Rect{key.x + 1, key.y, key.width - 2, key.height - 1}, glow > 0.0f ? kTab : kDim, glow > 0.0f ? 4 + static_cast<int>(glow * 10.0f) : 8);
      g.Stroke(key, kFaint);
    }
  }
  g.Text(left + 2, rect.y + rect.height - 9, NoteName(baseNote), kDim);
}

void DrawVoice(Gfx& g, const Controller& controller, Animator& animator, const Rect& area)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto* engine = host.Engine(state.track);
  const std::string name = engine != nullptr ? engine->CurrentPresetName() : std::string("NO ENGINE");
  const Font font = Gfx::Measure(name, Font::Huge) <= area.width ? Font::Huge : Font::Large;
  // The preset name decodes whenever it changes (SUB / ADD, browser, track switch).
  const std::uint32_t changed = std::max(animator.PresetChangedAt(state.track), animator.TrackChangedAt());
  fx::DecodeText(g, area.x, area.y + 2, name, kInk, font, fx::Progress(now, changed, 420), changed / 7 + now / 60, area.width);
  const int infoY = area.y + (font == Font::Huge ? 27 : 21);
  int x = g.Text(area.x, infoY, EngineShortName(host.EngineKind(state.track)), kTab);
  if (engine != nullptr && engine->CurrentPresetId().rfind("user.", 0) == 0) x = g.Text(x + 6, infoY, "USER", kFn);
  if (host.EngineKind(state.track) != synth::SynthEngineKind::Drums)
  {
    const KeyboardSettings& keys = state.keyboard;
    std::string detail = std::string(kLayoutNames[static_cast<std::size_t>(keys.layout)]);
    if (keys.layout != KeyboardLayout::Piano) detail += std::string(" ") + kNoteNames[static_cast<std::size_t>(keys.root)];
    if (keys.layout == KeyboardLayout::Scale) detail += std::string(" ") + kScales[keys.scale].label;
    detail += "  OCT " + std::string(keys.octave > 0 ? "+" : "") + std::to_string(keys.octave);
    g.Text(x + 8, infoY, detail, kDim);
  }
  const int pianoTop = infoY + 12;
  const Rect lower{area.x, pianoTop, area.width, area.y + area.height - pianoTop};
  if (host.EngineKind(state.track) == synth::SynthEngineKind::Drums)
  {
    DrawPads(g, controller, animator, lower, engine);
    return;
  }
  const int base = synth::kHomePerformanceBaseMidiNote + state.keyboard.octave * 12;
  DrawMiniPiano(g, lower, controller, animator, base);
}

void DrawKeyMap(Gfx& g, const Controller& controller, Animator& animator, const Rect& area)
{
  const int cellWidth = area.width / static_cast<int>(kPadColumns);
  const int cellHeight = area.height / static_cast<int>(kRows);
  const auto& keys = controller.State().keyboard;
  const std::uint32_t now = animator.Now();
  for (std::size_t row = 0; row < kRows; ++row)
  {
    for (std::size_t column = 0; column < kPadColumns; ++column)
    {
      const Rect cell{area.x + static_cast<int>(column) * cellWidth, area.y + static_cast<int>(row) * cellHeight, cellWidth - 2, cellHeight - 2};
      int note = 0;
      if (!controller.PadNoteFor(row, kPadFirstColumn + column, note))
      {
        g.Dither(cell, kFaint, 2);
        continue;
      }
      const bool held = controller.IsPadDown(row, kPadFirstColumn + column);
      const float glow = fx::Decay(now, animator.NoteHitAt(note), kAfterglowMs);
      const bool root = keys.layout == KeyboardLayout::Piano ? note % 12 == 0 : (note - keys.root) % 12 == 0;
      if (held) g.Fill(cell, kInk);
      else
      {
        if (glow > 0.0f) g.Dither(cell, kInk, static_cast<int>(glow * 10.0f));
        if (root) g.Stroke(cell, kDim);
        else g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
      }
      g.TextFit(cell.x + 2, cell.y + (cell.height - 7) / 2, cell.width - 3, NoteName(note), held ? kBlack : root ? kInk : kDim);
    }
  }
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
    case PageGraphic::KeyMap:
      DrawKeyMap(g, controller, animator, area);
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
    case PageGraphic::Waveform:
    {
      const auto sampler = host.Sampler().BuildUiState(track);
      const Rect graph{area.x + 2, area.y + 10, area.width - 4, area.height - 10};
      std::string title = sampler.hasSample ? sampler.sampleName : std::string("NO SAMPLE - FILE OR REC");
      if (sampler.hasSample && sampler.sampleRate > 0)
      {
        char seconds[16];
        std::snprintf(seconds, sizeof(seconds), "  %.2fS", static_cast<double>(sampler.frameCount) / static_cast<double>(sampler.sampleRate));
        title += seconds;
      }
      g.TextFit(area.x + 2, area.y, area.width - 4, title, sampler.hasSample ? kDim : ((now / 600) % 2 ? kFaint : kDim));
      const float cursor = sampler.frameCount > 0 ? static_cast<float>(sampler.previewCursorFrame) / static_cast<float>(sampler.frameCount) : 0.0f;
      widgets::Waveform(g, graph, sampler.waveform.data(), sampler.hasSample ? sampler.waveform.size() : 0,
                        Glide(animator, track, engine, "sample.start"), Glide(animator, track, engine, "sample.end", 1.0f), cursor, sampler.previewing);
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

void DrawStepCell(Gfx& g, Animator& animator, const Rect& rect, std::size_t index, const synth::SequencerStepUiState& step, bool inRange,
                  bool beat, bool drums, float phaseInStep, float rewind)
{
  const std::uint32_t now = animator.Now();
  if (!inRange)
  {
    g.Pixel(rect.x + rect.width / 2, rect.y + rect.height / 2, kShade);
    if (step.hasEvents) g.HLine(rect.x + 3, rect.y + rect.height - 2, rect.width - 6, kFaint);
    return;
  }
  const bool selected = step.held;
  const bool playing = step.playing;
  const float trail = playing ? 0.0f : std::max(fx::Decay(now, animator.StepPlayedAt(index), kTrailMs), rewind);
  const float released = selected ? 0.0f : fx::Decay(now, animator.DeselectedAt(index), 260);
  const float stamp = fx::Decay(now, animator.StepChangedAt(index), kStampMs);
  Color ink = kInk;
  if (selected)
  {
    g.Fill(rect, kInk);
    ink = kBlack;
  }
  else if (trail > 0.0f)
  {
    // Afterglow of the playhead (and of the rewind after a stop).
    g.Dither(rect, kPlay, static_cast<int>(trail * trail * 7.0f));
  }
  if (released > 0.0f)
  {
    // Ending of a selection: the inverse block dissolves and its frame contracts.
    g.Dither(rect, kInk, static_cast<int>(released * released * 13.0f));
    const int inset = static_cast<int>((1.0f - released) * 4.0f);
    g.Stroke(Rect{rect.x - 2 + inset, rect.y - 2 + inset, rect.width + 4 - inset * 2, rect.height + 4 - inset * 2}, kDim);
  }
  if (playing)
  {
    g.Dither(Rect{rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2}, kPlay, selected ? 0 : 3);
    // The playhead sweeps across the cell in time with the clock.
    const int sweep = rect.x + 1 + static_cast<int>(fx::Clamp01(phaseInStep) * static_cast<float>(rect.width - 3));
    g.VLine(sweep, rect.y + 1, rect.height - 2, kPlay);
    if (selected) g.Stroke(Rect{rect.x + 1, rect.y + 1, rect.width - 2, rect.height - 2}, kPlay);
    else
    {
      g.Stroke(rect, kPlay);
      ink = kPlay;
    }
  }
  if (!selected && !playing)
  {
    if (step.hasEvents) g.Fill(Rect{rect.x, rect.y + rect.height - 2, rect.width, 2}, kDim);
    else g.DottedHLine(rect.x, rect.y + rect.height - 1, rect.width, beat ? kDim : kFaint, 1);
  }
  // A note written or removed stamps the cell: a hard inverse flash that dissolves.
  if (stamp > 0.55f && !selected)
  {
    g.Fill(rect, kInk);
    ink = kBlack;
  }
  else if (stamp > 0.0f && !selected) g.Dither(rect, kInk, static_cast<int>(stamp * 14.0f));
  g.Text(rect.x + 3, rect.y + 2, Pad2(index + 1), ink == kBlack ? kBlack : beat ? kDim : kFaint);
  if (step.hasEvents)
  {
    const std::string_view drum = drums ? DrumName(step.lowestNote) : std::string_view{};
    std::string label = drum.empty() ? NoteName(step.lowestNote) : std::string(drum);
    if (step.noteCount > 1) label += "+" + std::to_string(step.noteCount - 1);
    g.TextFit(rect.x + 3, rect.y + 12, rect.width - 5, label, ink, Font::Small);
  }
  if (selected) fx::MarchingAnts(g, Rect{rect.x - 2, rect.y - 2, rect.width + 4, rect.height + 4}, kInk, now);
}

void InstrumentScene(Gfx& g, const Controller& controller, Animator& animator, std::uint8_t pageIndex)
{
  const InstrumentPageList list = controller.InstrumentPages();
  const std::size_t index = std::min<std::size_t>(pageIndex, list.count - 1);
  const InstrumentPage& page = list.pages[index];
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const bool current = index == controller.CurrentPage();
  const std::uint8_t focus = state.instrumentFocus[std::min<std::size_t>(index, state.instrumentFocus.size() - 1)];
  for (std::size_t slotIndex = 0; slotIndex < kGridCount; ++slotIndex)
  {
    const Slot& slot = page.slots[slotIndex];
    const Rect cell = Cell(slotIndex / kGridColumns, slotIndex % kGridColumns);
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
    const std::uint32_t key = Animator::Key("instr-focus", static_cast<std::uint32_t>(state.track) * 16u + static_cast<std::uint32_t>(index));
    const int x = static_cast<int>(std::lround(animator.Smooth(key, static_cast<float>(target.x), 50.0f)));
    const int y = static_cast<int>(std::lround(animator.Smooth(key + 1, static_cast<float>(target.y), 50.0f)));
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

void SequenceScene(Gfx& g, const Controller& controller, Animator& animator, std::uint8_t page)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = controller.HostView().Sequencer();
  const auto seq = sequencer.BuildUiState(state.track);
  const auto& track = seq.tracks[state.track];
  const bool drums = controller.HostView().EngineKind(state.track) == synth::SynthEngineKind::Drums;
  switch (page)
  {
    case 0:
    {
      const float position = seq.patternProgress * static_cast<float>(std::max<std::size_t>(1, track.stepCount));
      const float phaseInStep = position - std::floor(position);
      // Stop: a green head runs back from the last step to step 1.
      const float rewindT = animator.StopAt() != 0 && now - animator.StopAt() < kRewindMs ? fx::Progress(now, animator.StopAt(), kRewindMs) : 1.0f;
      const float head = static_cast<float>(animator.StopStep()) * (1.0f - fx::EaseOutCubic(rewindT));
      for (std::size_t index = 0; index < kGridCount; ++index)
      {
        float rewind = 0.0f;
        if (rewindT < 1.0f && index <= animator.StopStep())
        {
          const float distance = static_cast<float>(index) - head;
          if (distance >= -0.5f) rewind = std::max(0.0f, 1.0f - distance / 3.0f) * (1.0f - rewindT);
        }
        DrawStepCell(g, animator, Cell(index / kGridColumns, index % kGridColumns), index, seq.steps[index], index < track.stepCount,
                     index % 4 == 0, drums, phaseInStep, rewind);
        GridRipple(g, animator, index, seq.steps[index].held ? kInk : kTab);
      }
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
      const std::array<float, 4> targets{{-1.0f, static_cast<float>(track.stepCount - 1) / 15.0f,
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
        g.Text(table.x, y, "T" + std::to_string(row + 1), currentTrack ? kTab : kDim, Font::Small);
        g.Text(table.x + 22, y + 1, EngineShortName(host.EngineKind(row)), ink);
        char pattern[16];
        std::snprintf(pattern, sizeof(pattern), "P%02u/%02u", static_cast<unsigned>(summary.activePattern + 1), static_cast<unsigned>(summary.patternCount));
        g.Text(table.x + 74, y + 1, pattern, ink);
        // Step ruler: one tick per step, the playhead in green with a short trail.
        for (std::size_t step = 0; step < kGridCount; ++step)
        {
          const int x = table.x + 120 + static_cast<int>(step) * 5;
          const bool inRange = step < summary.stepCount;
          const bool head = seq.playing && step == summary.currentStep && inRange;
          const bool trail = seq.playing && inRange && summary.stepCount > 1 &&
                             (summary.currentStep + summary.stepCount - step) % summary.stepCount == 1;
          Color color = inRange ? (currentTrack ? kDim : kFaint) : kShade;
          if (trail) color = kFaint;
          g.Fill(Rect{x, y + 2, 3, 5}, head ? kPlay : color);
          if (trail) g.Dither(Rect{x, y + 2, 3, 5}, kPlay, 6);
        }
        if (!summary.enabled) g.Text(table.x + table.width - Gfx::Measure("OFF"), y + 1, "OFF", kRec);
        const float activity = animator.MeterPeak(row);
        if (activity > 0.02f) g.Dither(Rect{table.x - 5, y + 1, 2, 8}, kPlay, static_cast<int>(activity * 16.0f));
      }
      break;
    }
  }
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
      const std::array<const char*, 4> actions{{"SAVE", "SAVE AS", "LOAD", "NEW"}};
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
      for (std::size_t index = 4; index < 7; ++index) GridRipple(g, animator, index, kInk);
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
        g.Text(tx, ty, "T" + std::to_string(track + 1), activity > 0.3f ? kInk : kTab);
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
    case 2:
    default:
    {
      const std::array<const char*, 3> labels{{"KEY LED", "SCREEN", "VOLUME"}};
      const std::array<int, 3> values{{host.LedBrightness(), host.Backlight(), host.MasterVolume()}};
      const std::array<int, 3> maxima{{15, 255, 100}};
      for (std::size_t index = 0; index < labels.size(); ++index)
      {
        const float target = static_cast<float>(values[index]) / static_cast<float>(maxima[index]);
        widgets::ParamCell(g, Cell(0, index), labels[index], std::to_string(values[index]), animator.Smooth(Animator::Key(labels[index], 77u), target),
                           index == state.systemFocus ? CellStyle::Focused : CellStyle::Normal, 0.0f, now);
        GridRipple(g, animator, index, kInk);
      }
      const SystemInfo info = host.QuerySystem();
      const Rect panel = CellSpan(1, 0, 3, 4);
      int y = panel.y + 4;
      int line = 0;
      // Lines type out one after another, like a terminal printing a report.
      const float typed = fx::Clamp01(static_cast<float>(now - animator.CurrentTransition().start) / 600.0f);
      auto row = [&](const char* label, const std::string& value, Color color) {
        const float reveal = fx::Clamp01(typed * 5.0f - static_cast<float>(line));
        g.Text(panel.x, y, label, kDim);
        fx::TypeText(g, panel.x + 60, y, value, color, Font::Micro, reveal);
        y += 11;
        ++line;
      };
      row("DEVICE", "CARDENZA GROOVEBOX", kInk);
      row("BACKEND", info.backend, kInk);
      row("SD CARD", info.storageReady ? "READY" : "NOT FOUND", info.storageReady ? kPlay : kRec);
      row("MEMORY", info.freeHeap == 0 ? std::string("N/A") :
          std::to_string(info.freeHeap / 1024) + "K FREE  MIN " + std::to_string(info.minimumFreeHeap / 1024) + "K", kInk);
      row("BUILD", std::string(__DATE__), kDim);
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
      for (int trail = 1; trail <= 8; ++trail) g.Dither(Rect{head - trail, span.y, 1, span.height}, kPlay, 10 - trail);
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
        g.Fill(Rect{cell.x, cell.y + cell.height - 2, cell.width, 2}, playingRow ? kPlay : kDim);
      else g.DottedHLine(cell.x, cell.y + cell.height - 1, cell.width, kFaint);
      if (track == 0)
      {
        // Row number and length (bars) ride in the first track's cell.
        char label[16];
        std::snprintf(label, sizeof(label), "%02u", static_cast<unsigned>(row + 1));
        g.Text(cell.x + 3, cell.y + 2, label, cursor ? kBlack : (playingRow ? kPlay : kDim));
        std::snprintf(label, sizeof(label), "x%u", static_cast<unsigned>(songRow.repeats));
        g.Text(cell.x + cell.width - Gfx::Measure(label) - 2, cell.y + 2, label, cursor ? kBlack : kFn);
      }
      else g.Text(cell.x + 3, cell.y + 2, "T" + std::to_string(track + 1), cursor ? kBlack : kFaint);
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
    const Rect top = Cell(0, track);
    const Rect bottom = Cell(3, track);
    const bool focused = state.mixFocus == track;
    const bool audible = host.TrackAudible(track);
    const Rect fader{top.x + 1, top.y + 1, 9, bottom.y + bottom.height - top.y - 2};
    const float level = animator.Smooth(Animator::Key("mix-level", static_cast<std::uint32_t>(track)), static_cast<float>(host.TrackLevel(track)) / 100.0f);
    widgets::Fader(g, fader, level, audible ? kInk : kFaint, focused);
    if (focused) fx::MarchingAnts(g, Rect{fader.x - 1, fader.y - 1, fader.width + 2, fader.height + 2}, kTab, now);
    // Activity meter with a falling peak marker.
    const float activity = animator.Smooth(Animator::Key("mix-act", static_cast<std::uint32_t>(track)), host.TrackActivity(track), 35.0f);
    const int meter = static_cast<int>(activity * static_cast<float>(fader.height));
    const Rect meterRect{fader.x + fader.width + 2, fader.y + fader.height - meter, 2, meter};
    g.Fill(meterRect, audible ? kPlay : kFaint);
    const int peakY = fader.y + fader.height - static_cast<int>(animator.MeterPeak(track) * static_cast<float>(fader.height));
    if (animator.MeterPeak(track) > 0.02f) g.HLine(fader.x + fader.width + 1, peakY, 4, audible ? kInk : kFaint);

    const int x = top.x + 17;
    const int width = top.x + top.width - x;
    const std::string label = "T" + std::to_string(track + 1);
    if (focused)
    {
      g.Fill(Rect{x - 2, top.y, width + 2, 10}, kTab);
      g.Text(x, top.y + 2, label, kBlack);
    }
    else g.Text(x, top.y + 2, label, kTab);
    g.TextFit(x + 15, top.y + 2, width - 15, EngineCode(host.EngineKind(track)), focused ? kBlack : kDim);
    g.Text(x, top.y + 13, std::to_string(host.TrackLevel(track)), audible ? kInk : kDim, Font::Small);

    const auto badge = [&](std::size_t row) { const Rect cell = Cell(row, track); return Rect{x - 2, cell.y + 4, width + 2, 13}; };
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

// SENDS: one column per track (like MIX), one row per effect: filter, chorus, delay send, reverb
// send. A cell glows at its right edge while its track sounds into an open send.
void SendsScene(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const std::size_t row = index / kGridColumns;
    const std::size_t track = index % kGridColumns;
    const Rect cell = Cell(row, track);
    const auto* engine = host.Engine(track);
    const std::string label = "T" + std::to_string(track + 1) + " " + kSendTags[row];
    synth::SynthParameterInfo info;
    if (engine == nullptr || !engine->GetParameterInfo(kSendIds[row], info))
    {
      widgets::ParamCell(g, cell, label, "--", -1.0f, CellStyle::Disabled);
      continue;
    }
    const bool focused = state.sendsFocus == index;
    const bool off = info.value == info.defaultValue;
    const CellStyle style = focused ? CellStyle::Focused : off ? CellStyle::Disabled : CellStyle::Normal;
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
  const Rect focus = Cell(state.sendsFocus / kGridColumns, state.sendsFocus % kGridColumns);
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

void SceneContent(Gfx& g, const Controller& controller, Animator& animator, Scene scene, std::uint8_t page)
{
  switch (scene)
  {
    case Scene::Instrument: InstrumentScene(g, controller, animator, page); break;
    case Scene::Sequence: SequenceScene(g, controller, animator, page); break;
    case Scene::Tape:
      if (page == kTapeSong) TapeScene(g, controller, animator);
      else if (page == kTapeMix) MixScene(g, controller, animator);
      else if (page == kTapeSends) SendsScene(g, controller, animator);
      else if (page == kTapeFx) FxScene(g, controller, animator);
      else ProjectScene(g, controller, animator, static_cast<std::uint8_t>(page - kTapeProject));
      break;
  }
}

}  // namespace groove::render
