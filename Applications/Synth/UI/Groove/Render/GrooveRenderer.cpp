#include "Applications/Synth/UI/Groove/Render/GrooveRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Applications/Synth/UI/Groove/Render/Effects.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"
#include "Applications/Synth/UI/Groove/Render/Widgets.h"

namespace groove {

namespace {

using namespace groove::theme;

constexpr std::array<Color, 4> kStripColors{{kEsc, kTab, kFn, kCtrl}};
const Rect kContentArea{kContentX - 2, kContentY - 2, kContentWidth + 4, kContentHeight + 4};

Rect StripRect(std::size_t row)
{
  const int pitch = kScreenHeight / 4 + 1;  // 34
  return Rect{0, static_cast<int>(row) * pitch, kStripWidth, pitch - 3};
}

// A key cap at the screen edge: dithered body, a solid edge facing the content, clipped corners.
void DrawKeyCap(Gfx& g, const Rect& cap, Color color, int level, bool edgeOnRight)
{
  g.Dither(cap, color, level);
  g.VLine(edgeOnRight ? cap.x + cap.width - 1 : cap.x, cap.y, cap.height, color);
  g.Pixel(cap.x, cap.y, kBlack);
  g.Pixel(cap.x + cap.width - 1, cap.y, kBlack);
  g.Pixel(cap.x, cap.y + cap.height - 1, kBlack);
  g.Pixel(cap.x + cap.width - 1, cap.y + cap.height - 1, kBlack);
}

// The four left caps stand for the four left keys, row for row. Each shows what its key selects:
// ESC the scenes, TAB the tracks, FN the record arm (FN + PLAY), CTRL the pages of this screen.
void DrawStrips(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const bool recArmed = controller.HostView().Sequencer().RecordArmed();
  for (std::size_t row = 0; row < 4; ++row)
  {
    const Rect cap = StripRect(row);
    const Color color = kStripColors[row];
    const auto& key = state.left[row];
    DrawKeyCap(g, cap, color, key.down ? 9 : 3, true);
    if (key.down)
    {
      // Pressed: the cap fills from the bottom in a quick sweep.
      const float fill = fx::EaseOutCubic(fx::Progress(now, key.pressedAt == 0 ? 1 : key.pressedAt, 110));
      const int height = static_cast<int>(fill * static_cast<float>(cap.height));
      g.Fill(Rect{cap.x, cap.y + cap.height - height, cap.width, height}, color);
    }
    std::size_t count = 0;
    std::size_t current = 0;
    switch (row)
    {
      case 0: count = kSceneCount; current = static_cast<std::size_t>(state.scene); break;
      case 1: count = kTrackCount; current = state.track; break;
      case 2: count = 1; current = recArmed ? 0 : 1; break;
      case 3:
      default:
        count = std::min<std::size_t>(controller.PageCount(state.scene), 14);
        current = std::min<std::size_t>(controller.CurrentPage(), count - 1);
        break;
    }
    // One solid dot per thing the key selects, in the key's color; the current one is white. Long
    // page lists pack tighter.
    const int pitch = count > 9 ? 2 : 3;
    const int top = cap.y + (cap.height - static_cast<int>(count) * pitch + 1) / 2;
    for (std::size_t dot = 0; dot < count; ++dot)
    {
      const Rect mark{cap.x + 2, top + static_cast<int>(dot) * pitch, 4, pitch - 1};
      g.Fill(Rect{mark.x - 1, mark.y - 1, mark.width + 2, mark.height + 2}, kBlack);
      g.Fill(mark, key.down ? kBlack : color);
    }
    if (current < count)
    {
      // The current dot glides when the selection moves.
      const float target = static_cast<float>(top + static_cast<int>(current) * pitch);
      const int y = static_cast<int>(std::lround(animator.Smooth(Animator::Key("cap-dot", static_cast<std::uint32_t>(row)), target, 45.0f)));
      g.Fill(Rect{cap.x + 1, y - 1, 6, pitch + 1}, kBlack);
      g.Fill(Rect{cap.x + 2, y, 4, pitch - 1}, row == 2 ? kRec : kInk);
    }
  }
}

// The four right caps stand for ADD, SUB, ENTER and PLAY, top to bottom.
void DrawGlyphColumn(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const auto& sequencer = controller.HostView().Sequencer();
  const bool playing = sequencer.Playing();
  const bool recArmed = sequencer.RecordArmed();
  const std::array<Glyph, 4> glyphs{{Glyph::Plus, Glyph::Minus, Glyph::Enter, Glyph::Play}};
  for (std::size_t row = 0; row < 4; ++row)
  {
    const Rect strip = StripRect(row);
    const Rect cap{kGlyphColumnX, strip.y, kScreenWidth - kGlyphColumnX, strip.height};
    const bool pressed = (row == 0 && state.repeat.down && state.repeat.direction > 0) ||
                         (row == 1 && state.repeat.down && state.repeat.direction < 0) ||
                         (state.keyPressedAt[KeyIndex(row, kColumns - 1)] != 0 && now - state.keyPressedAt[KeyIndex(row, kColumns - 1)] < 90);
    Color body = kDim;
    int level = 2;
    Glyph glyph = glyphs[row];
    Color ink = kDim;
    if (row == 2 && controller.TopModal() != nullptr)
    {
      level = 3 + static_cast<int>(3.0f * (0.5f + 0.5f * std::sin(static_cast<float>(now) / 220.0f)));
      ink = kInk;
    }
    if (row == 3)
    {
      if (playing)
      {
        body = recArmed ? kRec : kPlay;
        level = 4 + static_cast<int>(fx::Decay(now, animator.BeatAt(), 180) * 8.0f);
        ink = body;
      }
      else if (recArmed)
      {
        glyph = Glyph::Rec;
        body = kRec;
        ink = (now / 300) % 2 ? kRec : kFaint;
      }
    }
    if (pressed)
    {
      g.Fill(cap, row == 3 && playing ? body : kInk);
      g.DrawGlyph(cap.x + (cap.width - Gfx::GlyphWidth(glyph)) / 2, cap.y + (cap.height - 7) / 2, glyph, kBlack);
      continue;
    }
    DrawKeyCap(g, cap, body, level, false);
    const int y = cap.y + (cap.height - 7) / 2 - (row == 3 ? static_cast<int>(fx::Decay(now, animator.PlayToggledAt(), 260) * 3.0f) : 0);
    const int x = cap.x + (cap.width - Gfx::GlyphWidth(glyph)) / 2;
    g.Fill(Rect{x - 1, y - 1, Gfx::GlyphWidth(glyph) + 2, 9}, kBlack);
    g.DrawGlyph(x, y, glyph, ink);
  }
}

void DrawHeader(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto seq = host.Sequencer().BuildUiState(state.track);

  // Scene label decodes in with every scene change.
  const auto& transition = animator.CurrentTransition();
  const float sceneDecode = transition.kind == Animator::TransitionKind::Scene ? fx::Progress(now, transition.start, 320) : 1.0f;
  int x = kContentX;
  const char sceneNumber[2] = {static_cast<char>('1' + static_cast<int>(state.scene)), 0};
  x = g.Text(x, kHeaderY, sceneNumber, kEsc);
  x = fx::DecodeText(g, x + 3, kHeaderY, kSceneNames[static_cast<std::size_t>(state.scene)], kInk, Font::Micro, sceneDecode, now / 40);
  x += 6;

  // Track label: inverse flash on a track change, a pulse on every note played.
  const std::string track = "T" + std::to_string(state.track + 1);
  const float trackFlash = fx::Decay(now, animator.TrackChangedAt(), 360);
  const float notePulse = fx::Decay(now, animator.LastNoteAt(), 160);
  if (trackFlash > 0.4f || notePulse > 0.5f)
  {
    g.Fill(Rect{x - 1, 0, Gfx::Measure(track) + 2, 9}, kTab);
    x = g.Text(x, kHeaderY, track, kBlack);
  }
  else x = g.Text(x, kHeaderY, track, kTab);
  x += 4;
  const auto* engine = host.Engine(state.track);
  x = g.Text(x, kHeaderY, EngineShortName(host.EngineKind(state.track)), kDim);
  x += 4;

  // Right cluster: REC, transport, BPM (pulses on the beat), unsaved marker.
  char bpm[8];
  std::snprintf(bpm, sizeof(bpm), "%u", static_cast<unsigned>(seq.bpm));
  int right = kContentRight;
  if (state.dirty)
  {
    g.Text(right - 6, kHeaderY, "*", (now / 700) % 2 ? kFn : kDim);
    right -= 9;
  }
  right -= Gfx::Measure(bpm);
  const float beat = seq.playing ? fx::Decay(now, animator.BeatAt(), 150) : 0.0f;
  g.Text(right, kHeaderY, bpm, beat > 0.3f ? (animator.Downbeat() ? kPlay : kInk) : (seq.playing ? kInk : kDim));
  right -= 9;
  if (seq.playing) g.DrawGlyph(right, kHeaderY, Glyph::Play, kPlay);
  else g.DrawGlyph(right, kHeaderY, Glyph::Stop, kFaint);
  right -= 4;
  if (seq.recordArmed)
  {
    right -= Gfx::Measure("REC") + 2;
    if ((now / 250) % 2 || !seq.playing) g.Text(right, kHeaderY, "REC", kRec);
  }
  if (seq.songMode && seq.songLength > 0)
  {
    char song[12];
    std::snprintf(song, sizeof(song), "R%02u", static_cast<unsigned>(seq.songRow + 1));
    right -= Gfx::Measure(song) + 4;
    g.Text(right, kHeaderY, song, kFn);
  }
  if (engine != nullptr)
  {
    const std::uint32_t changed = std::max(animator.PresetChangedAt(state.track), animator.TrackChangedAt());
    fx::DecodeText(g, x, kHeaderY, engine->CurrentPresetName(), kInk, Font::Micro, fx::Progress(now, changed, 380), changed,
                   right - x - 6);
  }

  // Rule: dotted; while playing, the loop (or song) progress with a comet head.
  g.DottedHLine(kContentX, kHeaderRuleY, kContentWidth, kFaint);
  if (seq.songPlaying && seq.songLength > 0)
  {
    // Song mode: the rule is the whole tape, one tick per row, the head at the song position.
    for (std::size_t row = 0; row <= seq.songLength; ++row)
      g.Pixel(kContentX + static_cast<int>(row * static_cast<std::size_t>(kContentWidth) / seq.songLength), kHeaderRuleY - 1, kFn);
    const float position = (static_cast<float>(seq.songRow) + seq.songRowProgress) / static_cast<float>(seq.songLength);
    const int progress = static_cast<int>(position * static_cast<float>(kContentWidth));
    g.HLine(kContentX, kHeaderRuleY, std::max(1, progress), kFn);
    g.Fill(Rect{kContentX + progress - 1, kHeaderRuleY - 1, 3, 3}, kInk);
  }
  else if (seq.playing)
  {
    const int progress = static_cast<int>(seq.patternProgress * static_cast<float>(kContentWidth));
    g.HLine(kContentX, kHeaderRuleY, std::max(1, progress), kPlay);
    for (int tail = 1; tail <= 10; ++tail) g.Dither(Rect{kContentX + progress - tail, kHeaderRuleY - 1, 1, 1}, kPlay, 12 - tail);
    g.Fill(Rect{kContentX + progress - 1, kHeaderRuleY - 1, 3, 3}, kInk);
  }
  else if (animator.StopAt() != 0 && now - animator.StopAt() < kRewindMs)
  {
    // Stop: the progress line rewinds to the start with its comet head.
    const float rewind = fx::EaseOutCubic(fx::Progress(now, animator.StopAt(), kRewindMs));
    const int progress = static_cast<int>(animator.StopProgress() * (1.0f - rewind) * static_cast<float>(kContentWidth));
    g.HLine(kContentX, kHeaderRuleY, std::max(1, progress), kPlay);
    for (int tail = 1; tail <= 10; ++tail) g.Dither(Rect{kContentX + progress + tail, kHeaderRuleY - 1, 1, 1}, kPlay, 12 - tail);
    g.Fill(Rect{kContentX + progress - 1, kHeaderRuleY - 1, 3, 3}, kInk);
  }
}

// Page tabs. When they do not all fit, a window of tabs around the current page is shown with
// arrows where more pages continue, and a counter (current / total) on the right.
void DrawTabs(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const std::size_t count = controller.PageCount(state.scene);
  const std::size_t current = std::min<std::size_t>(controller.CurrentPage(), count - 1);
  std::vector<std::string> tabs;
  std::vector<int> widths;
  int total = 0;
  for (std::size_t page = 0; page < count; ++page)
  {
    tabs.push_back(controller.PageTab(state.scene, page));
    widths.push_back(Gfx::Measure(tabs.back()) + 7);
    total += widths.back();
  }
  std::size_t first = 0;
  std::size_t last = count - 1;
  int left = kContentX;
  int right = kContentRight;
  if (total > kContentWidth)
  {
    char counter[24];
    std::snprintf(counter, sizeof(counter), "%02u/%02u", static_cast<unsigned>(current + 1), static_cast<unsigned>(count));
    g.Text(kContentRight - Gfx::Measure(counter), kFooterY, counter, kFaint);
    left = kContentX + 7;
    right = kContentRight - Gfx::Measure(counter) - 9;
    // Grow the window from the current tab, alternating right and left, while it fits.
    first = last = current;
    int used = widths[current];
    for (bool grew = true; grew;)
    {
      grew = false;
      if (last + 1 < count && used + widths[last + 1] <= right - left) { used += widths[++last]; grew = true; }
      if (first > 0 && used + widths[first - 1] <= right - left) { used += widths[--first]; grew = true; }
    }
    if (first > 0) g.Text(kContentX, kFooterY, "<", kCtrl);
    if (last + 1 < count) g.Text(right + 2, kFooterY, ">", kCtrl);
  }
  std::vector<int> positions(count, 0);
  int x = left;
  for (std::size_t page = first; page <= last; ++page)
  {
    positions[page] = x;
    x += widths[page];
  }
  // The violet highlight glides to the current tab.
  const std::uint32_t key = Animator::Key("tabs", static_cast<std::uint32_t>(state.scene) * 7 + static_cast<std::uint32_t>(state.track));
  const float boxX = animator.Smooth(key, static_cast<float>(positions[current] - 2), 55.0f);
  const float boxW = animator.Smooth(key + 1, static_cast<float>(Gfx::Measure(tabs[current]) + 4), 55.0f);
  g.Fill(Rect{static_cast<int>(std::lround(boxX)), kFooterY - 1, static_cast<int>(std::lround(boxW)), 9}, kCtrl);
  for (std::size_t page = first; page <= last; ++page)
  {
    const int tabX = positions[page];
    const int tabW = Gfx::Measure(tabs[page]);
    const bool covered = tabX >= boxX - 1 && tabX + tabW <= boxX + boxW + 1;
    g.Text(tabX, kFooterY, tabs[page], covered ? kBlack : (page == current ? kInk : kDim));
  }
}

void DrawFooter(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  g.DottedHLine(kContentX, kFooterY - 3, kContentWidth, kShade, 1);
  auto drawToast = [&](const std::string& text, ToastKind kind, int lift, float typed, int shake) {
    const Color color = kind == ToastKind::Error ? kRec : kind == ToastKind::Ok ? kPlay : kInk;
    g.SetClip(Rect{kContentX, kFooterY - 2, kContentWidth, 11});
    const float pulse = kind == ToastKind::Error ? 0.5f + 0.5f * std::sin(static_cast<float>(now) / 70.0f) : 1.0f;
    g.Fill(Rect{kContentX, kFooterY - 2 + lift, kContentWidth, 11}, kBlack);
    g.Dither(Rect{kContentX + shake, kFooterY - 1 + lift, 4, 9}, color, static_cast<int>(6.0f + 6.0f * pulse));
    fx::TypeText(g, kContentX + 8 + shake, kFooterY + lift, text.substr(0, static_cast<std::size_t>(kContentWidth / 6 - 2)), color, Font::Micro, typed);
    g.ResetClip();
  };
  if (!state.toast.text.empty())
  {
    // Toast slides up from below and types itself out; an error shakes first.
    const float t = fx::Progress(now, animator.ToastAt(), 220);
    const int lift = static_cast<int>((1.0f - fx::EaseOutBack(t)) * 9.0f);
    const std::uint32_t age = now - animator.ToastAt();
    const int shake = state.toast.kind == ToastKind::Error && age < 260
        ? static_cast<int>(std::sin(static_cast<float>(age) / 18.0f) * 4.0f * (1.0f - static_cast<float>(age) / 260.0f)) : 0;
    drawToast(state.toast.text, state.toast.kind, lift, fx::Progress(now, animator.ToastAt(), 260), shake);
    return;
  }
  const float toastExit = fx::Progress(now, animator.ToastEndedAt(), kToastExitMs);
  const bool exiting = animator.ToastEndedAt() != 0 && toastExit < 1.0f;
  if (const Modal* modal = controller.TopModal())
  {
    g.TextFit(kContentX, kFooterY, kContentWidth, render::ModalHint(controller, *modal), kDim);
    return;
  }
  const auto& sequencer = controller.HostView().Sequencer();
  if (state.scene == Scene::Sequence && controller.CurrentPage() == 0)
  {
    const auto seq = sequencer.BuildUiState(state.track);
    if (seq.hasHeldSteps)
    {
      // Step inspector: lane chooser (ENTER) with a gliding highlight, scope and value.
      static constexpr std::array<const char*, 4> kLanes{{"NOTE", "LEN", "VEL", "TIME"}};
      std::array<int, 4> laneX{};
      int x = kContentX;
      for (std::size_t lane = 0; lane < kLanes.size(); ++lane)
      {
        laneX[lane] = x;
        x += Gfx::Measure(kLanes[lane]) + 6;
      }
      const std::size_t lane = static_cast<std::size_t>(seq.editLane);
      const float boxX = animator.Smooth(Animator::Key("lane"), static_cast<float>(laneX[lane] - 2), 45.0f);
      const float boxW = animator.Smooth(Animator::Key("lane", 1), static_cast<float>(Gfx::Measure(kLanes[lane]) + 4), 45.0f);
      g.Fill(Rect{static_cast<int>(std::lround(boxX)), kFooterY - 1, static_cast<int>(std::lround(boxW)), 9}, kInk);
      for (std::size_t index = 0; index < kLanes.size(); ++index)
      {
        const bool covered = laneX[index] >= boxX - 1 && laneX[index] + Gfx::Measure(kLanes[index]) <= boxX + boxW + 1;
        g.Text(laneX[index], kFooterY, kLanes[index], covered ? kBlack : kFaint);
      }
      std::string notes = seq.noteNames;
      std::string scope = seq.scopeLabel;
      if (controller.HostView().EngineKind(state.track) == synth::SynthEngineKind::Drums)
      {
        const auto& primary = seq.steps[seq.primaryHeldStep];
        notes = primary.hasEvents ? std::string(DrumName(primary.lowestNote)) : std::string{};
        if (seq.noteCount > 1) notes += "+" + std::to_string(seq.noteCount - 1);
        if (scope != "ALL" && seq.scopedNote >= 0 && !DrumName(seq.scopedNote).empty()) scope = std::string(DrumName(seq.scopedNote));
      }
      std::string detail = seq.valueLabel.empty() ? notes : seq.valueLabel;
      if (seq.editLane == synth::SequencerEditLane::Note && !scope.empty()) detail = scope + " " + notes;
      g.TextFit(x + 4, kFooterY, kContentRight - x - 4, detail, kInk);
      return;
    }
  }
  if (state.scene == Scene::Tape && controller.CurrentPage() == kTapeMix)
  {
    const Host& host = controller.HostView();
    const bool focused = state.mixFocus >= kTrackCount;
    g.Text(kContentX, kFooterY, "MASTER", focused ? kInk : kDim);
    if (focused) fx::MarchingAnts(g, Rect{kContentX - 2, kFooterY - 2, Gfx::Measure("MASTER") + 4, 11}, kInk, now);
    const float master = animator.Smooth(Animator::Key("master"), static_cast<float>(host.MasterVolume()) / 100.0f);
    widgets::LevelBar(g, Rect{kContentX + 44, kFooterY + 1, 90, 5}, master, focused ? kInk : kDim, kFaint);
    g.Text(kContentX + 138, kFooterY, std::to_string(host.MasterVolume()), focused ? kInk : kDim);
    g.Text(kContentX + 162, kFooterY, "OUT", kFaint);
    const float peak = animator.OutputPeakHold();
    const Rect meter{kContentX + 182, kFooterY + 1, 36, 5};
    widgets::LevelBar(g, meter, host.OutputPeak(), host.OutputPeak() > 0.95f ? kRec : kPlay, kFaint);
    g.VLine(meter.x + static_cast<int>(peak * static_cast<float>(meter.width - 1)), meter.y - 1, meter.height + 2, peak > 0.95f ? kRec : kInk);
    return;
  }
  DrawTabs(g, controller, animator);
  if (exiting)
  {
    // Ending: the toast un-types and drops out, uncovering the tabs.
    const int drop = static_cast<int>(fx::EaseInOutCubic(toastExit) * 11.0f);
    drawToast(animator.EndedToast(), animator.EndedToastKind(), drop, 1.0f - toastExit, 0);
  }
}

// Hold maps cover the content area with tiles laid out exactly like the GRID keys under the right
// hand: the tile under a key is what that key does while the left key stays down.
const Rect kMapArea{kContentX - 2, kContentY - 2, kContentWidth + 4, kContentHeight + 4};

// The map grows out of the strip of the key that summoned it, over a darkening scrim.
void BeginOverlay(Gfx& g, std::size_t index, Rect& panel, Color accent, float t)
{
  fx::Fade(g, kContentArea, static_cast<int>(t * static_cast<float>(kDitherScrim)));
  const Rect strip = StripRect(index);
  const int originY = strip.y + strip.height / 2;
  const Rect grown{kStripWidth + 2 + static_cast<int>(static_cast<float>(panel.x - kStripWidth - 2) * t),
                   originY + static_cast<int>(static_cast<float>(panel.y - originY) * t),
                   std::max(1, static_cast<int>(static_cast<float>(panel.width) * t)),
                   std::max(1, static_cast<int>(static_cast<float>(panel.height) * t))};
  g.Fill(grown, kBlack);
  g.Stroke(grown, accent);
  // A tether from the strip to the panel while it grows.
  if (t < 1.0f) g.HLine(kStripWidth, originY, grown.x - kStripWidth, accent);
  panel = grown;
  g.SetClip(grown);
}

// The header line names the map and how to use it while the map is up.
void DrawMapTitle(Gfx& g, const std::string& title, const std::string& hint, Color accent, float t, std::uint32_t now)
{
  g.Fill(Rect{kContentX - 1, 0, kContentWidth + 2, kHeaderRuleY + 1}, kBlack);
  const int x = fx::DecodeText(g, kContentX, kHeaderY, title, accent, Font::Micro, t * 1.5f, now / 50);
  g.TextFit(std::max(x + 8, kContentRight - Gfx::Measure(hint)), kHeaderY, kContentRight - x - 8, hint, kDim);
  g.HLine(kContentX, kHeaderRuleY, static_cast<int>(t * static_cast<float>(kContentWidth)), accent);
}

// Tiles drop into place one after another along the GRID diagonal.
Rect DropIn(const Rect& rect, float t, std::size_t order)
{
  const float tileT = fx::EaseOutBack(fx::Clamp01(t * 1.8f - static_cast<float>(order) * 0.09f));
  return Rect{rect.x, rect.y - static_cast<int>((1.0f - tileT) * 8.0f), rect.width, rect.height};
}

// One map tile. `current` is drawn inverse with crawling ants in the map's colour; a tile the key
// cannot use right now stays dotted.
void MapTile(Gfx& g, const Rect& tile, bool current, bool usable, Color accent, std::uint32_t now)
{
  if (current)
  {
    g.Fill(tile, kInk);
    fx::MarchingAnts(g, Rect{tile.x - 2, tile.y - 2, tile.width + 4, tile.height + 4}, accent, now);
    return;
  }
  g.Fill(tile, kBlack);
  if (usable)
  {
    g.Dither(tile, kFaint, 2);
    g.Stroke(tile, kDim);
  }
  else
  {
    g.DottedHLine(tile.x, tile.y, tile.width, kShade);
    g.DottedHLine(tile.x, tile.y + tile.height - 1, tile.width, kShade);
  }
}

void PressRipple(Gfx& g, Animator& animator, std::size_t index, const Rect& tile, Color accent)
{
  const std::uint32_t pressed = animator.GridPressedAt(index);
  if (pressed != 0 && animator.Now() - pressed < 300) fx::Ripple(g, tile, fx::Progress(animator.Now(), pressed, 300), accent);
}

// ESC: three tall tiles, one per GRID column, each listing its scene's pages.
void DrawSceneOverlay(Gfx& g, const Controller& controller, Animator& animator, float t)
{
  const std::uint32_t now = animator.Now();
  const UiState& state = controller.State();
  Rect panel = kMapArea;
  DrawMapTitle(g, "ESC  SCENES", "GRID COLUMN = SCENE", kEsc, t, now);
  BeginOverlay(g, 0, panel, kEsc, t);
  for (std::size_t scene = 0; scene < kSceneCount; ++scene)
  {
    const Rect tile = DropIn(widgets::CellSpan(0, scene, 4, 1), t, scene * 2);
    const bool current = static_cast<std::size_t>(state.scene) == scene;
    MapTile(g, tile, current, true, kEsc, now);
    const Color ink = current ? kBlack : kInk;
    const char number[2] = {static_cast<char>('1' + scene), 0};
    g.Text(tile.x + 4, tile.y + 4, number, current ? kBlack : kEsc, Font::Large);
    g.Text(tile.x + 4, tile.y + 21, kSceneNames[scene], ink, Font::Small);
    // The pages of that scene, the one it opens on marked.
    const Scene target = static_cast<Scene>(scene);
    const std::size_t count = controller.PageCount(target);
    const std::size_t open = state.page[scene];
    const std::size_t lines = static_cast<std::size_t>((tile.height - 36) / 8);
    const std::size_t first = count > lines && open >= lines ? std::min(open - lines / 2, count - lines) : 0;
    for (std::size_t line = 0; line < lines && first + line < count; ++line)
    {
      const std::size_t pageIndex = first + line;
      const int y = tile.y + 34 + static_cast<int>(line) * 8;
      const bool marked = pageIndex == std::min(open, count - 1);
      if (marked) g.Fill(Rect{tile.x + 2, y - 1, tile.width - 4, 8}, current ? kBlack : kDim);
      g.TextFit(tile.x + 4, y, tile.width - 8, controller.PageTab(target, pageIndex), marked ? (current ? kInk : kBlack) : (current ? kBlack : kDim));
    }
    for (std::size_t row = 0; row < kRows; ++row) PressRipple(g, animator, row * kGridColumns + scene, tile, kEsc);
  }
  // The fourth column explains the rest of the key.
  const Rect help = DropIn(widgets::CellSpan(0, 3, 4, 1), t, 6);
  MapTile(g, help, false, false, kEsc, now);
  int y = help.y + 6;
  for (const char* line : {"TAP:", "BACK", "", "HOLD +", "COLUMN:", "SCENE"})
  {
    g.Text(help.x + 4, y, line, kFaint);
    y += 9;
  }
  g.ResetClip();
}

// TAB: one column per track, laid out like MIX: select, mute, solo, sequencer.
void DrawTrackOverlay(Gfx& g, const Controller& controller, Animator& animator, float t)
{
  const Host& host = controller.HostView();
  const std::uint32_t now = animator.Now();
  const auto seq = host.Sequencer().BuildUiState(controller.State().track);
  Rect panel = kMapArea;
  DrawMapTitle(g, "TAB  TRACKS", "ADD/SUB = NEXT/PREV", kTab, t, now);
  BeginOverlay(g, 1, panel, kTab, t);
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    const bool current = track == controller.State().track;
    const Rect tile = DropIn(widgets::Cell(0, track), t, track);
    MapTile(g, tile, current, true, kTab, now);
    const Color ink = current ? kBlack : kInk;
    g.Text(tile.x + 3, tile.y + 2, "T" + std::to_string(track + 1), current ? kBlack : kTab);
    g.Text(tile.x + 18, tile.y + 2, EngineCode(host.EngineKind(track)), current ? kBlack : kDim);
    if (const auto* engine = host.Engine(track)) g.TextFit(tile.x + 3, tile.y + 11, tile.width - 6, engine->CurrentPresetName(), ink);
    const float level = animator.Smooth(Animator::Key("ovl-level", static_cast<std::uint32_t>(track)), static_cast<float>(host.TrackLevel(track)) / 100.0f);
    widgets::LevelBar(g, Rect{tile.x + 3, tile.y + tile.height - 3, tile.width - 10, 2}, level, ink, current ? kBlack : kFaint);
    const float activity = animator.MeterPeak(track);
    if (activity > 0.02f) g.Dither(Rect{tile.x + tile.width - 5, tile.y + 2, 3, tile.height - 4}, current ? kBlack : kPlay, static_cast<int>(activity * 16.0f));
    PressRipple(g, animator, track, tile, kTab);

    const std::array<bool, 3> states{{host.TrackMuted(track), host.TrackSoloed(track), seq.tracks[track].enabled}};
    const std::array<const char*, 3> names{{"MUTE", "SOLO", "SEQ"}};
    const std::array<Color, 3> colors{{kRec, kFn, kDim}};
    for (std::size_t kind = 0; kind < 3; ++kind)
    {
      const Rect cell = DropIn(widgets::Cell(kind + 1, track), t, track + kind + 1);
      MapTile(g, cell, false, true, kTab, now);
      widgets::Badge(g, Rect{cell.x + 3, cell.y + 5, cell.width - 6, 13}, names[kind], states[kind], colors[kind]);
      PressRipple(g, animator, (kind + 1) * kGridColumns + track, cell, kTab);
    }
  }
  g.ResetClip();
}

// CTRL: every page of the current scene on its GRID key.
void DrawPageOverlay(Gfx& g, const Controller& controller, Animator& animator, float t)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  const std::size_t count = std::min(controller.PageCount(state.scene), kGridCount);
  const std::size_t current = controller.CurrentPage();
  Rect panel = kMapArea;
  char title[32];
  std::snprintf(title, sizeof(title), "CTRL  %s PAGES", kSceneNames[static_cast<std::size_t>(state.scene)]);
  DrawMapTitle(g, title, "TAP CTRL = NEXT", kCtrl, t, now);
  BeginOverlay(g, 3, panel, kCtrl, t);
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const std::size_t row = index / kGridColumns;
    const std::size_t column = index % kGridColumns;
    const Rect tile = DropIn(widgets::Cell(row, column), t, row + column);
    if (index >= count)
    {
      g.Pixel(tile.x + tile.width / 2, tile.y + tile.height / 2, kShade);
      continue;
    }
    const bool isCurrent = index == current;
    MapTile(g, tile, isCurrent, true, kCtrl, now);
    // The page number sits top right when the tab leaves room for it.
    const std::string tab = controller.PageTab(state.scene, index);
    char number[12];
    std::snprintf(number, sizeof(number), "%02u", static_cast<unsigned>(index + 1));
    // Long tabs drop to the small terminal font rather than truncate.
    const Font tabFont = Gfx::Measure(tab, Font::Small) <= tile.width - 5 ? Font::Small : Font::Micro;
    if (Gfx::Measure(tab, tabFont) + Gfx::Measure(number) + 8 <= tile.width)
      g.Text(tile.x + tile.width - Gfx::Measure(number) - 2, tile.y + 2, number, isCurrent ? kBlack : kCtrl);
    g.TextFit(tile.x + 3, tile.y + (tabFont == Font::Small ? 2 : 3), tile.width - 5, tab, isCurrent ? kBlack : kInk, tabFont);
    g.TextFit(tile.x + 3, tile.y + 14, tile.width - 5, controller.PageName(state.scene, index), isCurrent ? kBlack : kDim);
    PressRipple(g, animator, index, tile, kCtrl);
  }
  g.ResetClip();
}

// FN: the GRID functions on their keys; the letter shortcuts on the pads are listed in the header
// and footer lines.
void DrawFnOverlay(Gfx& g, const Controller& controller, Animator& animator, float t)
{
  const std::uint32_t now = animator.Now();
  const auto functions = controller.FnGridEntries();
  const auto letters = controller.FnEntries();
  // The letter shortcuts fill the header line, then the footer line, in priority order: key in
  // yellow, what it does in white. Whatever does not fit is left to the key lights and the docs.
  g.Fill(Rect{kContentX - 1, 0, kContentWidth + 2, kHeaderRuleY + 1}, kBlack);
  g.Fill(Rect{kContentX - 1, kFooterY - 2, kContentWidth + 2, 11}, kBlack);
  g.HLine(kContentX, kHeaderRuleY, static_cast<int>(t * static_cast<float>(kContentWidth)), kFn);
  const std::array<int, 2> lineY{{kHeaderY, kFooterY}};
  std::array<int, 2> lineX{{g.Text(kContentX, kHeaderY, "FN", kFn) + 8, kContentX}};
  const std::size_t shown = static_cast<std::size_t>(fx::Clamp01(t * 1.3f) * static_cast<float>(letters.size()) + 0.5f);
  std::size_t drawn = 0;
  for (const auto& entry : letters)
  {
    if (!entry.available) continue;
    const int width = Gfx::Measure(entry.key) + 6 + Gfx::Measure(entry.label);
    for (std::size_t line = 0; line < lineX.size(); ++line)
    {
      if (lineX[line] + width > kContentRight) continue;
      if (drawn++ < shown)
      {
        const int labelX = g.Text(lineX[line], lineY[line], entry.key, kFn) + 6;
        g.Text(labelX, lineY[line], entry.label, kInk);
      }
      lineX[line] += width + 9;
      break;
    }
  }

  Rect panel = kMapArea;
  BeginOverlay(g, 2, panel, kFn, t);
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    const std::size_t row = index / kGridColumns;
    const std::size_t column = index % kGridColumns;
    const Rect tile = DropIn(widgets::Cell(row, column), t, row + column);
    const auto& entry = functions[index];
    if (entry.label == nullptr)
    {
      g.Pixel(tile.x + tile.width / 2, tile.y + tile.height / 2, kShade);
      continue;
    }
    MapTile(g, tile, false, entry.available, kFn, now);
    const char legend[2] = {LegendChar(row, kGridFirstColumn + column), 0};
    g.Text(tile.x + tile.width - Gfx::Measure(legend) - 2, tile.y + 2, legend, entry.available ? kFn : kShade);
    const std::string label = entry.label;
    static constexpr std::array<std::pair<const char*, Glyph>, 4> kArrows{{{"UP", Glyph::Up}, {"DOWN", Glyph::Down}, {"LEFT", Glyph::Left}, {"RIGHT", Glyph::Right}}};
    bool drawn = false;
    for (const auto& arrow : kArrows)
      if (label == arrow.first)
      {
        g.DrawGlyph(tile.x + (tile.width - Gfx::GlyphWidth(arrow.second)) / 2, tile.y + 9, arrow.second, entry.available ? kInk : kFaint);
        drawn = true;
      }
    if (!drawn) g.TextFit(tile.x + 3, tile.y + 9, tile.width - 5, label, entry.available ? kInk : kFaint, Font::Small);
    PressRipple(g, animator, index, tile, kFn);
  }
  g.ResetClip();
}

void DrawContent(Gfx& g, const Controller& controller, Animator& animator)
{
  const UiState& state = controller.State();
  const std::uint32_t now = animator.Now();
  g.SetClip(kContentArea);
  if (const Modal* modal = controller.TopModal())
  {
    render::ModalContent(g, controller, animator, *modal);
    // Modals unfold from the centre line between two bright scan lines.
    const float t = fx::EaseOutCubic(fx::Progress(now, animator.ModalOpenedAt(), kModalOpenMs));
    if (t < 1.0f)
    {
      const int middle = kContentArea.y + kContentArea.height / 2;
      const int half = static_cast<int>(t * static_cast<float>(kContentArea.height / 2));
      g.Fill(Rect{kContentArea.x, kContentArea.y, kContentArea.width, middle - half - kContentArea.y}, kBlack);
      g.Fill(Rect{kContentArea.x, middle + half, kContentArea.width, kContentArea.y + kContentArea.height - middle - half}, kBlack);
      g.HLine(kContentArea.x, middle - half, kContentArea.width, kInk);
      g.HLine(kContentArea.x, middle + half, kContentArea.width, kInk);
    }
    if (const Modal* closing = animator.ClosingModal(); closing != nullptr && closing != modal)
    {
      const float fold = fx::EaseInOutCubic(fx::Progress(now, animator.ClosingModalAt(), kModalCloseMs));
      const int middle = kContentArea.y + kContentArea.height / 2;
      const int half = static_cast<int>((1.0f - fold) * static_cast<float>(kContentArea.height / 2));
      if (half > 0 && animator.ClosingModalAt() > animator.ModalOpenedAt())
      {
        const Rect band{kContentArea.x, middle - half, kContentArea.width, half * 2};
        g.SetClip(band);
        g.Fill(band, kBlack);
        render::ModalContent(g, controller, animator, *closing);
        g.SetClip(kContentArea);
        g.HLine(band.x, band.y, band.width, kInk);
        g.HLine(band.x, band.y + band.height - 1, band.width, kInk);
      }
    }
    g.ResetClip();
    return;
  }

  const auto& transition = animator.CurrentTransition();
  if (animator.TransitionActive())
  {
    const float p = fx::EaseInOutCubic(fx::Progress(now, transition.start, transition.duration));
    const bool sceneChange = transition.kind == Animator::TransitionKind::Scene;
    const int distance = sceneChange ? kContentWidth + 14 : kContentWidth / 2 + 10;
    const int direction = transition.direction;
    const int outgoing = -direction * static_cast<int>(p * static_cast<float>(distance));
    const int incoming = direction * static_cast<int>((1.0f - p) * static_cast<float>(distance));
    g.SetOrigin(outgoing, 0);
    render::SceneContent(g, controller, animator, transition.fromScene, transition.fromPage);
    g.SetOrigin(0, 0);
    if (!sceneChange)
    {
      // Pages overlap: the outgoing page sinks into the dither while the new one covers it.
      fx::Fade(g, kContentArea, static_cast<int>(p * 14.0f));
      const int left = direction > 0 ? kContentArea.x + incoming : kContentArea.x;
      const int right = direction > 0 ? kContentArea.x + kContentArea.width : kContentArea.x + kContentArea.width + incoming;
      g.Fill(Rect{left, kContentArea.y, right - left, kContentArea.height}, kBlack);
    }
    g.SetOrigin(incoming, 0);
    render::SceneContent(g, controller, animator, state.scene, controller.CurrentPage());
    g.SetOrigin(0, 0);
    // The seam rides the leading edge of the incoming screen.
    const int seamX = direction > 0 ? kContentArea.x + incoming : kContentArea.x + kContentArea.width + incoming - 1;
    fx::Seam(g, seamX, kContentArea.y, kContentArea.height, direction, sceneChange ? kEsc : kCtrl);
    if (sceneChange && p < 0.45f) fx::Glitch(g, kContentArea, (0.45f - p) * 2.2f, now / 16);
  }
  else
  {
    render::SceneContent(g, controller, animator, state.scene, controller.CurrentPage());
  }

  // Ending: a closed modal folds shut toward the centre line over whatever lies beneath.
  if (const Modal* closing = animator.ClosingModal())
  {
    const float t = fx::EaseInOutCubic(fx::Progress(now, animator.ClosingModalAt(), kModalCloseMs));
    const int middle = kContentArea.y + kContentArea.height / 2;
    const int half = static_cast<int>((1.0f - t) * static_cast<float>(kContentArea.height / 2));
    if (half > 0)
    {
      const Rect band{kContentArea.x, middle - half, kContentArea.width, half * 2};
      g.SetClip(band);
      g.Fill(band, kBlack);
      render::ModalContent(g, controller, animator, *closing);
      g.SetClip(kContentArea);
      g.HLine(band.x, band.y, band.width, kInk);
      g.HLine(band.x, band.y + band.height - 1, band.width, kInk);
    }
  }

  // Track change: a cyan scan line refreshes the content top to bottom.
  const float wipe = fx::Progress(now, animator.TrackChangedAt(), kTrackWipeMs);
  if (animator.TrackChangedAt() != 0 && wipe < 1.0f)
  {
    const int y = kContentArea.y + static_cast<int>(fx::EaseOutCubic(wipe) * static_cast<float>(kContentArea.height));
    fx::Fade(g, Rect{kContentArea.x, y, kContentArea.width, kContentArea.y + kContentArea.height - y}, 11);
    fx::ScanLine(g, kContentArea.x, kContentArea.width, y, 6, kTab);
  }
  g.ResetClip();
}

void DrawRecordVignette(Gfx& g, const Controller& controller, Animator& animator)
{
  const auto& sequencer = controller.HostView().Sequencer();
  if (!sequencer.RecordArmed() || !sequencer.Playing()) return;
  const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(animator.Now()) / 140.0f);
  const int level = 3 + static_cast<int>(pulse * 6.0f);
  g.Dither(Rect{kContentX - 3, kContentY - 3, kContentWidth + 6, 2}, kRec, level);
  g.Dither(Rect{kContentX - 3, kContentBottom + 1, kContentWidth + 6, 2}, kRec, level);
  g.Dither(Rect{kContentX - 3, kContentY - 3, 2, kContentHeight + 6}, kRec, level);
  g.Dither(Rect{kContentRight + 1, kContentY - 3, 2, kContentHeight + 6}, kRec, level);
}

void DrawBoot(Gfx& g, Animator& animator)
{
  const std::uint32_t now = animator.Now();
  const std::uint32_t elapsed = now - animator.BootAt();
  const float t = static_cast<float>(elapsed) / static_cast<float>(kBootMs);
  // Scan line sweeping a noise field, the logotype decoding behind it.
  for (int y = 0; y < kScreenHeight; y += 3)
  {
    const std::uint32_t noise = fx::Hash(static_cast<std::uint32_t>(y) * 131u + now / 33);
    g.Dither(Rect{0, y, kScreenWidth, 2}, kShade, static_cast<int>(noise % 4));
  }
  const char* title = "CARDENZA";
  const int titleX = (kScreenWidth - Gfx::Measure(title, Font::Huge)) / 2;
  fx::DecodeText(g, titleX, 40, title, kInk, Font::Huge, fx::Clamp01(t * 2.2f - 0.1f), now / 45);
  const char* subtitle = "GROOVEBOX";
  const int subtitleX = (kScreenWidth - Gfx::Measure(subtitle)) / 2;
  fx::DecodeText(g, subtitleX, 70, subtitle, kTab, Font::Micro, fx::Clamp01(t * 2.6f - 0.8f), now / 45);
  // Four colored dashes: the four hidden keys, landing one by one.
  for (std::size_t index = 0; index < 4; ++index)
  {
    const float landed = fx::EaseOutBack(fx::Clamp01(t * 3.0f - 0.9f - static_cast<float>(index) * 0.18f));
    const int x = kScreenWidth / 2 - 38 + static_cast<int>(index) * 20;
    const int y = 88 + static_cast<int>((1.0f - landed) * 20.0f);
    if (landed > 0.0f) g.Dither(Rect{x, y, 16, 4}, kStripColors[index], 10);
  }
  const int scan = static_cast<int>(fx::Clamp01(t * 1.4f) * static_cast<float>(kScreenHeight + 10));
  fx::ScanLine(g, 0, kScreenWidth, scan, 8, kDim);
}

}  // namespace

void RenderFrame(Gfx& g, const Controller& controller, Animator& animator)
{
  const std::uint32_t now = animator.Now();
  g.Clear(kBlack);
  const std::uint32_t sinceBoot = now - animator.BootAt();
  if (sinceBoot < kBootMs - 300)
  {
    DrawBoot(g, animator);
    return;
  }

  DrawStrips(g, controller, animator);
  DrawGlyphColumn(g, controller, animator);
  DrawHeader(g, controller, animator);
  DrawContent(g, controller, animator);
  DrawRecordVignette(g, controller, animator);
  DrawFooter(g, controller, animator);

  // Hold maps grow out of their strip while held and shrink back into it on release.
  const std::array<bool, 4> active{{controller.EscOverlay(), controller.TabOverlay(), controller.FnOverlay(), controller.CtrlOverlay()}};
  for (std::size_t index = 0; index < active.size(); ++index)
  {
    float t = 0.0f;
    if (active[index]) t = fx::EaseOutCubic(fx::Progress(now, animator.OverlayAt(index), kOverlayGrowMs));
    else if (animator.OverlayClosedAt(index) != 0 && now - animator.OverlayClosedAt(index) < kOverlayCloseMs)
      t = 1.0f - fx::EaseInOutCubic(fx::Progress(now, animator.OverlayClosedAt(index), kOverlayCloseMs));
    if (t <= 0.0f) continue;
    if (index == 0) DrawSceneOverlay(g, controller, animator, t);
    else if (index == 1) DrawTrackOverlay(g, controller, animator, t);
    else if (index == 2) DrawFnOverlay(g, controller, animator, t);
    else DrawPageOverlay(g, controller, animator, t);
  }

  // Session replaced (load / new): one bright scan refreshes the whole screen.
  const UiState& state = controller.State();
  const float session = fx::Progress(now, state.sessionAt, kSessionSweepMs);
  if (state.sessionAt != 0 && session < 1.0f)
  {
    const int y = static_cast<int>(fx::EaseInOutCubic(session) * static_cast<float>(kScreenHeight + 12));
    fx::Fade(g, Rect{kStripWidth + 1, y, kScreenWidth - kStripWidth - 1, kScreenHeight - y}, 12);
    fx::ScanLine(g, kStripWidth + 1, kScreenWidth - kStripWidth - 1, y, 10, kInk);
  }

  // Panic: the screen flares white, ALL STOP stamps the centre, and everything dies to black.
  const float panic = fx::Decay(now, state.panicAt, kPanicMs);
  if (panic > 0.0f)
  {
    g.Dither(Rect{0, 0, kScreenWidth, kScreenHeight}, kInk, static_cast<int>(panic * panic * 14.0f));
    const Rect stamp{kScreenWidth / 2 - 62, kScreenHeight / 2 - 15, 124, 30};
    g.Fill(stamp, kBlack);
    g.Stroke(stamp, kRec);
    g.TextAligned(stamp, "ALL STOP", panic > 0.5f || (now / 60) % 2 ? kRec : kInk, Font::Large, Align::Center);
  }

  // Boot hand-off: the interface resolves out of black through the Bayer thresholds.
  if (sinceBoot < kBootMs)
  {
    const float t = static_cast<float>(sinceBoot - (kBootMs - 300)) / 300.0f;
    fx::Fade(g, Rect{0, 0, kScreenWidth, kScreenHeight}, 16 - static_cast<int>(t * 16.0f));
  }
}

}  // namespace groove
