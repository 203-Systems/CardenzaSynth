#include "Applications/Synth/UI/Groove/Render/Animator.h"

#include "Applications/Synth/UI/Groove/Render/Effects.h"
#include "Applications/Synth/UI/Groove/Render/Theme.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace groove {

namespace {


// Direction of a page change: CTRL steps forward and wraps, Fn + CTRL steps back.
int PageDirection(std::size_t from, std::size_t to, std::size_t count)
{
  const int delta = static_cast<int>(to) - static_cast<int>(from);
  if (count > 2 && std::abs(delta) > static_cast<int>(count) / 2) return delta > 0 ? -1 : 1;
  return delta >= 0 ? 1 : -1;
}

}  // namespace

std::uint32_t Animator::Key(const char* text, std::uint32_t salt)
{
  std::uint32_t hash = 2166136261u ^ salt;
  for (const char* c = text; c != nullptr && *c != 0; ++c) hash = (hash ^ static_cast<std::uint8_t>(*c)) * 16777619u;
  return hash == 0 ? 1 : hash;
}

bool Animator::TransitionActive() const
{
  return transition_.kind != TransitionKind::None && now_ - transition_.start < transition_.duration;
}

float Animator::Smooth(std::uint32_t key, float target, float timeConstantMs)
{
  SmoothEntry* slot = nullptr;
  SmoothEntry* oldest = &smooth_[0];
  for (auto& entry : smooth_)
  {
    if (entry.used && entry.key == key) { slot = &entry; break; }
    if (!entry.used || entry.time < oldest->time) oldest = &entry;
  }
  if (slot == nullptr)
  {
    *oldest = SmoothEntry{key, target, now_, true};
    return target;
  }
  const float dt = static_cast<float>(now_ - slot->time);
  slot->time = now_;
  // A long gap (page not shown for a while) snaps instead of sweeping from a stale value.
  if (dt > 500.0f) slot->value = target;
  else slot->value += (target - slot->value) * (1.0f - std::exp(-dt / timeConstantMs));
  if (std::fabs(target - slot->value) < 0.002f) slot->value = target;
  return slot->value;
}

void Animator::Update(const Controller& controller, std::uint32_t now)
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const auto& sequencer = host.Sequencer();
  const auto seq = sequencer.BuildUiState(state.track);
  const float dt = lastFrame_ == 0 ? 16.0f : static_cast<float>(std::min<std::uint32_t>(now - lastFrame_, 100));
  lastFrame_ = now;
  now_ = now;
  UpdateStandby(controller, dt);

  if (!primed_)
  {
    primed_ = true;
    bootAt_ = now;
    scene_ = state.scene;
    pages_ = state.page;
    track_ = state.track;
    projectName_ = state.projectName;
    padNotes_ = state.padNote;
    for (std::size_t track = 0; track < kTrackCount; ++track)
    {
      if (const auto* engine = host.Engine(track)) presets_[track] = engine->CurrentPresetName();
      patterns_[track] = seq.tracks[track].activePattern;
    }
    fingerprintTrack_ = state.track;
    fingerprintPattern_ = seq.activePattern;
    for (std::size_t step = 0; step < kGridCount; ++step) fingerprints_[step] = seq.steps[step].contentFingerprint;
    return;
  }

  // Scene and page transitions.
  const bool trackChanged = state.track != track_;
  if (state.scene != scene_)
  {
    transition_ = Transition{TransitionKind::Scene, scene_, pages_[static_cast<std::size_t>(scene_)],
                             static_cast<int>(state.scene) > static_cast<int>(scene_) ? 1 : -1, now, kSceneSlideMs};
  }
  else if (!trackChanged && state.page[static_cast<std::size_t>(state.scene)] != pages_[static_cast<std::size_t>(state.scene)] &&
           controller.TopModal() == nullptr)
  {
    const std::size_t from = pages_[static_cast<std::size_t>(state.scene)];
    const std::size_t to = state.page[static_cast<std::size_t>(state.scene)];
    transition_ = Transition{TransitionKind::Page, state.scene, static_cast<std::uint8_t>(from),
                             PageDirection(from, to, controller.PageCount(state.scene)), now, kPageSlideMs};
  }
  scene_ = state.scene;
  pages_ = state.page;
  if (trackChanged)
  {
    trackChangedAt_ = now;
    track_ = state.track;
  }

  // Modals and hold overlays.
  const Modal* modal = controller.TopModal();
  const std::size_t depth = state.modals.size();
  const ModalKind kind = modal != nullptr ? modal->kind : ModalKind::None;
  if (depth > modalDepth_ || (depth > 0 && kind != modalKind_)) modalOpenedAt_ = now;
  // The closing modal is no longer in the controller, so keep a copy of the top one whenever it
  // changes; when it goes away, that copy folds shut.
  if ((depth < modalDepth_ || (depth == modalDepth_ && kind != modalKind_)) && lastModalValid_)
  {
    closingModal_ = lastModal_;
    closingModalAt_ = now;
  }
  if (modal != nullptr)
  {
    const std::uint32_t signature = static_cast<std::uint32_t>(depth) ^ static_cast<std::uint32_t>(modal->kind) << 4 ^
                                    static_cast<std::uint32_t>(modal->cursor) << 8 ^ static_cast<std::uint32_t>(modal->text.size()) << 16 ^
                                    static_cast<std::uint32_t>(modal->search.size()) << 21 ^ static_cast<std::uint32_t>(modal->items.size()) << 24 ^
                                    static_cast<std::uint32_t>(modal->trimMarker) << 31;
    if (!lastModalValid_ || signature != lastModalSignature_ || depth != modalDepth_)
    {
      // What the fold needs: the rows on screen, not a copy of a long list.
      static_cast<ModalFields&>(lastModal_) = *modal;
      lastModal_.itemCount = modal->Count();
      lastModal_.firstItem = ListFirstRow(modal->cursor);
      lastModal_.items.clear();
      for (std::size_t index = lastModal_.firstItem; index < lastModal_.firstItem + kListVisibleRows; ++index)
      {
        if (const ListItem* row = modal->Row(index)) lastModal_.items.push_back(*row);
      }
      lastModalSignature_ = signature;
      lastModalValid_ = true;
    }
  }
  else lastModalValid_ = false;
  if (modal != nullptr && modal->kind == ModalKind::TextEntry)
  {
    if (modal->text.size() > textLength_) textTypedAt_ = now;
    textLength_ = modal->text.size();
  }
  else textLength_ = 0;
  modalDepth_ = depth;
  modalKind_ = kind;
  const std::array<bool, 4> overlay{{controller.EscOverlay(), controller.TabOverlay(), controller.FnOverlay(), controller.CtrlOverlay()}};
  for (std::size_t index = 0; index < overlay.size(); ++index)
  {
    if (overlay[index] && !overlay_[index]) overlayAt_[index] = now;
    if (!overlay[index] && overlay_[index]) overlayClosedAt_[index] = now;
    overlay_[index] = overlay[index];
  }
  if (!state.toast.text.empty() && state.toast.text != toast_) toastAt_ = now;
  if (state.toast.text.empty() && !toast_.empty())
  {
    endedToast_ = toast_;
    endedToastKind_ = toastKind_;
    toastEndedAt_ = now;
  }
  toast_ = state.toast.text;
  toastKind_ = state.toast.kind;
  if (state.projectName != projectName_) projectChangedAt_ = now;
  projectName_ = state.projectName;

  // Keys and notes.
  for (std::size_t index = 0; index < kGridCount; ++index)
  {
    if (state.gridDown[index] && !gridDown_[index]) gridPressedAt_[index] = now;
    gridDown_[index] = state.gridDown[index];
  }
  for (std::size_t index = 0; index < state.padNote.size(); ++index)
  {
    const std::uint8_t note = state.padNote[index];
    if (note != kNoNote && note != padNotes_[index] && note < 128)
    {
      // The key and every note of its chord.
      noteHitAt_[note] = now;
      for (std::size_t voice = 0; voice < state.padChordCount[index]; ++voice) noteHitAt_[state.padChord[index][voice] & 0x7F] = now;
      lastNoteAt_ = now;
    }
  }
  padNotes_ = state.padNote;
  // From the controller, not from the pads being down: a tap can be over within a frame.
  for (std::size_t lane = 0; lane < drumHits_.size(); ++lane)
    if (state.drumPadHits[lane].at > drumHits_[lane].at) drumHits_[lane] = state.drumPadHits[lane];

  // Sequencer: playhead flash, beats, content stamps, pattern switches.
  if (seq.playing != playing_) playToggledAt_ = now;
  if (!seq.playing && playing_)
  {
    stopAt_ = now;
    stopProgress_ = lastProgress_;
    stopStep_ = step_ % kGridCount;
  }
  playing_ = seq.playing;
  if (seq.playing) lastProgress_ = seq.patternProgress;
  if (seq.playing && seq.currentStep != step_)
  {
    if (seq.currentStep % 4 == 0)
    {
      beatAt_ = now;
      downbeat_ = seq.currentStep == 0;
    }
    // The notes the sequencer plays on this track light the pads (and the on-screen keyboards).
    std::array<std::uint8_t, 8> notes{};
    const std::size_t count = sequencer.StepNotes(seq.currentStep, notes);
    for (std::size_t index = 0; index < count; ++index) sequencedAt_[notes[index] & 0x7F] = now;
    if (host.EngineKind(state.track) == synth::SynthEngineKind::Drums)
    {
      std::array<synth::SynthSequencer::StepNoteView, 8> hits{};
      const std::size_t hitCount = sequencer.StepNoteEvents(state.track, seq.currentStep, hits);
      for (std::size_t slot = 0; slot < hitCount; ++slot)
        for (std::size_t lane = 0; lane < kDrumNotes.size(); ++lane)
          if (kDrumNotes[lane] == hits[slot].note)
            drumHits_[lane] = DrumHit{now == 0 ? 1 : now, static_cast<std::uint8_t>(DrumRowForVelocity(hits[slot].velocity)), hits[slot].velocity};
    }
  }
  if (seq.playing) step_ = seq.currentStep;
  // Every arpeggiator note glows on its pad (and on the on-screen keyboards) when it is on this track.
  if (seq.arpTick != arpTick_)
  {
    arpTick_ = seq.arpTick;
    if (seq.arpLastNote >= 0 && seq.arpTrack == state.track) sequencedAt_[static_cast<std::size_t>(seq.arpLastNote) & 0x7F] = now;
  }
  // Flipping the page shows other steps: nothing changed.
  const bool samePage = fingerprintPage_ == seq.viewPage;
  const bool samePattern = fingerprintTrack_ == state.track && fingerprintPattern_ == seq.activePattern && samePage;
  for (std::size_t step = 0; step < kGridCount; ++step)
  {
    const auto& cell = seq.steps[step];
    if (cell.playing && !stepPlaying_[step]) stepPlayedAt_[step] = now;
    stepPlaying_[step] = cell.playing;
    if (!cell.held && stepHeld_[step] && !trackChanged && samePage) deselectedAt_[step] = now;
    stepHeld_[step] = cell.held;
    if (samePattern && cell.contentFingerprint != fingerprints_[step]) stepChangedAt_[step] = now;
    fingerprints_[step] = cell.contentFingerprint;
  }
  fingerprintTrack_ = state.track;
  fingerprintPattern_ = seq.activePattern;
  fingerprintPage_ = seq.viewPage;
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    if (seq.tracks[track].activePattern != patterns_[track]) patternChangedAt_[track] = now;
    patterns_[track] = seq.tracks[track].activePattern;
    if (const auto* engine = host.Engine(track))
    {
      std::string preset = engine->CurrentPresetName();
      if (preset != presets_[track])
      {
        presetChangedAt_[track] = now;
        presets_[track] = std::move(preset);
      }
    }
  }

  // INSTR and the SETUP settings pages: flash a slot whose value changed (keyed per track and page so
  // switching does not flash).
  const InstrumentPage* settings = modal == nullptr ? controller.SettingsPageShown() : nullptr;
  if ((state.scene == Scene::Instrument && modal == nullptr) || settings != nullptr)
  {
    const InstrumentPage* page = settings != nullptr ? settings : controller.CurrentInstrumentPage();
    const std::uint32_t pageKey = settings != nullptr ? 0x5E700000u + controller.CurrentPage()
                                                     : static_cast<std::uint32_t>(state.track * 64 + controller.CurrentPage() + 1) ^
                                                           static_cast<std::uint32_t>(host.EngineKind(state.track)) << 16;
    for (std::size_t slot = 0; slot < kGridCount; ++slot)
    {
      synth::SynthParameterInfo info;
      std::string text;
      const int value = controller.SlotValue(page->slots[slot], info, text) ? info.value : 0;
      if (pageKey == slotPageKey_ && value != slotValues_[slot]) valueChangedAt_[slot] = now;
      slotValues_[slot] = value;
    }
    slotPageKey_ = pageKey;
  }
  else slotPageKey_ = 0;

  // Mixer toggles and meters with a falling peak hold.
  for (std::size_t track = 0; track < kTrackCount; ++track)
  {
    const std::array<bool, 3> flags{{host.TrackMuted(track), host.TrackSoloed(track), seq.tracks[track].enabled}};
    for (std::size_t kindIndex = 0; kindIndex < flags.size(); ++kindIndex)
    {
      if (flags[kindIndex] != mixFlags_[track * 3 + kindIndex]) mixToggledAt_[track * 3 + kindIndex] = now;
      mixFlags_[track * 3 + kindIndex] = flags[kindIndex];
    }
    const float activity = host.TrackActivity(track);
    meterPeak_[track] = std::max(activity, meterPeak_[track] - dt * 0.0012f);
  }
  outputPeak_ = std::max(host.OutputPeak(), outputPeak_ - dt * 0.0008f);
}

void Animator::DecorateLeds(const Controller& controller, const LedFrame& frame, std::array<std::uint8_t, kKeyCount>& levels) const
{
  const UiState& state = controller.State();
  const Host& host = controller.HostView();
  const std::uint32_t now = now_;
  auto raise = [&](std::size_t index, float level) {
    const int value = std::clamp(static_cast<int>(std::lround(level)), 0, 15);
    if (value > levels[index]) levels[index] = static_cast<std::uint8_t>(value);
  };

  // Standby: the key lights play their show instead of the state.
  if (state.standby)
  {
    StandbyLeds(controller, levels);
    return;
  }

  // Blink = waiting: on and off. The guide's hints the same at half the speed. Steady keys keep their level.
  const bool blinkOn = (now / 250) % 2 == 0;
  const bool hintOn = (now / 500) % 2 == 0;
  for (std::size_t index = 0; index < kKeyCount; ++index)
  {
    const LedCell& cell = frame[index];
    const bool off = (cell.effect == LedEffect::Blink && !blinkOn) || (cell.effect == LedEffect::Hint && !hintOn);
    levels[index] = off ? 0 : cell.level;
  }

  // Boot: two light waves sweep the keyboard, then the working lights fade in.
  const std::uint32_t sinceBoot = now - bootAt_;
  if (primed_ && sinceBoot < kBootMs)
  {
    const float t = static_cast<float>(sinceBoot) / static_cast<float>(kBootMs);
    const float fadeIn = fx::Clamp01((t - 0.8f) / 0.2f);
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = 0; column < kColumns; ++column)
      {
        const std::size_t index = KeyIndex(row, column);
        float wave = 0.0f;
        for (int pass = 0; pass < 2; ++pass)
        {
          const float front = (t * 1.8f - static_cast<float>(pass) * 0.45f) * 18.0f - 2.0f + static_cast<float>(row) * 0.6f;
          const float distance = static_cast<float>(column) - front;
          wave = std::max(wave, 15.0f * std::exp(-distance * distance / 3.0f));
        }
        levels[index] = static_cast<std::uint8_t>(std::max(wave, static_cast<float>(levels[index]) * fadeIn));
      }
    return;
  }

  // Panic: everything flares and dies away.
  const float panic = fx::Decay(now, state.panicAt, kPanicMs);
  if (panic > 0.0f)
    for (std::size_t index = 0; index < kKeyCount; ++index) levels[index] = static_cast<std::uint8_t>(std::lround(15.0f * panic * panic));

  // Session replaced: a bright row wipe top to bottom.
  const float session = fx::Progress(now, state.sessionAt, kSessionSweepMs);
  if (state.sessionAt != 0 && session < 1.0f)
  {
    const float front = session * 5.0f - 0.5f;
    for (std::size_t row = 0; row < kRows; ++row)
    {
      const float level = 15.0f * std::max(0.0f, 1.0f - std::fabs(static_cast<float>(row) - front) / 1.2f);
      for (std::size_t column = 0; column < kColumns; ++column) raise(KeyIndex(row, column), level);
    }
  }

  // Screen changes introduce the GRID block (it mirrors the 4x4 on screen): one sweep corner to
  // corner. The pads keep playing the same keyboard, so they stay as they are.
  if (TransitionActive())
  {
    const float p = fx::Progress(now, transition_.start, transition_.duration);
    const float gridFront = p * 9.0f - 1.0f;
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = 0; column < kGridColumns; ++column)
      {
        const float diagonal = transition_.direction > 0 ? static_cast<float>(row + column) : static_cast<float>(row + kGridColumns - 1 - column);
        const float distance = diagonal - gridFront;
        if (distance <= 0.0f && distance > -2.5f) raise(KeyIndex(row, kGridFirstColumn + column), 14.0f * (1.0f + distance / 2.5f));
      }
  }

  // Track change: the left column says which track is now active, top key = T1 ... bottom = T4.
  const float trackShown = fx::Decay(now, trackChangedAt_, 800);
  if (trackShown > 0.0f)
    for (std::size_t row = 0; row < kRows; ++row)
      levels[KeyIndex(row, 0)] = row == state.track ? 15 : static_cast<std::uint8_t>(static_cast<float>(levels[KeyIndex(row, 0)]) * (1.0f - trackShown));

  // Edge sweep: while nobody plays, a light now and then runs down both edge columns, row by row,
  // so the strips on screen (which mirror these keys) read as the keys beside them.
  std::uint32_t lastPress = 0;
  for (const std::uint32_t pressed : state.keyPressedAt) lastPress = std::max(lastPress, pressed);
  bool leftHeld = false;
  for (const auto& key : state.left) leftHeld = leftHeld || key.down;
  if (primed_ && now - bootAt_ >= kBootMs && !leftHeld && now - lastPress >= kEdgeSweepIdleMs)
  {
    const std::uint32_t phase = (now - bootAt_) % kEdgeSweepPeriodMs;
    for (std::size_t row = 0; row < kRows; ++row)
    {
      const std::uint32_t start = static_cast<std::uint32_t>(row) * 90u;
      if (phase < start || phase - start >= 260u) continue;
      const float glow = 15.0f * (1.0f - static_cast<float>(phase - start) / 260.0f);
      raise(KeyIndex(row, 0), glow);
      raise(KeyIndex(row, kColumns - 1), glow);
    }
  }

  // Hold maps: the GRID keys they accept come on corner to corner, mirroring the tiles on screen.
  for (std::size_t map = 0; map < overlay_.size(); ++map)
  {
    if (!overlay_[map]) continue;
    const std::uint32_t age = now - overlayAt_[map];
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = 0; column < kGridColumns; ++column)
        if (age < static_cast<std::uint32_t>(row + column) * 30u) levels[KeyIndex(row, kGridFirstColumn + column)] = 0;
  }

  const Modal* modal = controller.TopModal();
  const bool steps = state.scene == Scene::Sequence && controller.CurrentPage() == 0 && modal == nullptr;
  if (steps)
  {
    const float rewind = fx::Progress(now, stopAt_, kRewindMs);
    for (std::size_t step = 0; step < kGridCount; ++step)
    {
      const std::size_t index = KeyIndex(step / kGridColumns, kGridFirstColumn + step % kGridColumns);
      raise(index, 15.0f * fx::Decay(now, stepChangedAt_[step], 300));
      if (stopAt_ != 0 && rewind < 1.0f)
      {
        const float head = static_cast<float>(stopStep_) * (1.0f - fx::EaseOutCubic(rewind));
        const float distance = static_cast<float>(step) - head;
        if (distance >= -0.5f && step <= stopStep_) raise(index, 14.0f * std::max(0.0f, 1.0f - distance / 3.0f) * (1.0f - rewind));
      }
    }
  }
  if ((state.scene == Scene::Instrument || controller.SettingsPageShown() != nullptr) && modal == nullptr)
    for (std::size_t slot = 0; slot < kGridCount; ++slot)
      raise(KeyIndex(slot / kGridColumns, kGridFirstColumn + slot % kGridColumns), 15.0f * fx::Decay(now, valueChangedAt_[slot], 300));
  if (state.scene == Scene::Sequence && controller.CurrentPage() == 1 && modal == nullptr)
  {
    const auto seq = host.Sequencer().BuildUiState(state.track);
    const std::size_t active = seq.tracks[state.track].activePattern;
    raise(KeyIndex(active / kGridColumns, kGridFirstColumn + active % kGridColumns), 15.0f * fx::Decay(now, patternChangedAt_[state.track], 400));
  }

  // A note the sequencer plays on this track lights its pad full for a moment, like a press. Only
  // pads that are lit at all (keys that can sound), and not while a map or pop-up owns the pads.
  const bool padsPlay = modal == nullptr && !controller.CellPickerActive() && !controller.EscOverlay() && !controller.TabOverlay() && !controller.CtrlOverlay() &&
                        !state.left[2].down;
  const bool drums = host.EngineKind(state.track) == synth::SynthEngineKind::Drums;
  ledStruck_.fill(false);
  if (padsPlay)
    for (std::size_t row = 0; row < kRows; ++row)
      for (std::size_t column = kPadFirstColumn; column < kPadFirstColumn + kPadColumns; ++column)
      {
        int note = 0;
        const std::size_t index = KeyIndex(row, column);
        if (frame[index].level == 0 || !controller.PadNoteFor(row, column, note)) continue;
        if (drums)
        {
          // At rest the columns already say which way is harder: brighter towards the top. A hit runs
          // along its column and strikes, it does not glide in.
          static constexpr std::array<std::uint8_t, kRows> kRestLevels{{9, 8, 7, 6}};
          const std::size_t lane = column - kPadFirstColumn;
          if (levels[index] == kLedHalf) levels[index] = kRestLevels[row];
          // The bar of a held drum grows with the wave: a key waits until it arrives.
          else if (!controller.IsPadDown(row, column) && !DrumWaveArrived(lane, row)) levels[index] = kRestLevels[row];
          const float wave = DrumWave(lane, row);
          if (wave > 0.0f)
          {
            raise(index, 15.0f * wave);
            ledStruck_[index] = wave > 0.2f;
          }
          continue;
        }
        raise(index, 15.0f * fx::Decay(now, sequencedAt_[static_cast<std::size_t>(note)], 200));
      }

  // Error: the left column blinks twice.
  if (!toast_.empty() && toastKind_ == ToastKind::Error && now - toastAt_ < 360 && ((now - toastAt_) / 90) % 2 == 0)
    for (std::size_t row = 0; row < kRows; ++row) raise(KeyIndex(row, 0), 15.0f);

}

namespace {
constexpr float kDrumWaveRowMs = 28.0f;  // How fast a hit runs along its column.
constexpr float kDrumWaveFadeMs = 180.0f;
}  // namespace

bool Animator::DrumWaveArrived(std::size_t lane, std::size_t row) const
{
  if (lane >= drumHits_.size() || drumHits_[lane].at == 0) return true;
  const DrumHit& hit = drumHits_[lane];
  return static_cast<float>(now_ - hit.at) >= std::fabs(static_cast<float>(row) - static_cast<float>(hit.row)) * kDrumWaveRowMs;
}

float Animator::DrumWave(std::size_t lane, std::size_t row) const
{
  if (lane >= drumHits_.size() || drumHits_[lane].at == 0) return 0.0f;
  const DrumHit& hit = drumHits_[lane];
  const float age = static_cast<float>(now_ - hit.at);
  const float rows = std::fabs(static_cast<float>(row) - static_cast<float>(hit.row));
  const float arrival = rows * kDrumWaveRowMs;
  if (age < arrival || age > 900.0f) return 0.0f;
  const float power = static_cast<float>(hit.velocity) / 127.0f;
  const float reach = 1.0f - rows * (0.54f - 0.44f * power);  // Hard: all the way. Soft: a key or two.
  if (reach <= 0.0f) return 0.0f;
  // Every hit strikes hard where it lands; how hard it was shows in how far it goes.
  return reach * std::exp(-(age - arrival) / kDrumWaveFadeMs);
}

// ---------------------------------------------------------------------------------------------
// Standby

const char* StandbyWord(bool alt)
{
  return alt ? "203" : "CARDENZA";
}

int StandbyLogoWidth(bool alt)
{
  return Gfx::Measure(StandbyWord(alt), Font::Huge);
}

// The word, then the four colored dashes under it (the boot screen's four hidden keys).
int StandbyLogoHeight()
{
  return Gfx::CapHeight(Font::Huge) + 8;
}

void Animator::UpdateStandby(const Controller& controller, float dt)
{
  StandbyScene& scene = standby_;
  if (!controller.Standby())
  {
    scene.startedAt = 0;
    return;
  }
  const float width = static_cast<float>(theme::kScreenWidth);
  const float height = static_cast<float>(theme::kScreenHeight);
  const float bodyHeight = static_cast<float>(StandbyLogoHeight());
  if (scene.startedAt != controller.StandbyAt())
  {
    // A new standby: CARDENZA somewhere in the upper half, 203 in the lower, drifting diagonally.
    scene = StandbyScene{};
    scene.startedAt = controller.StandbyAt();
    scene.seed = fx::Hash(scene.startedAt * 2654435761u + 0x5bd1e995u);
    for (std::size_t index = 0; index < scene.bodies.size(); ++index)
    {
      StandbyBody& body = scene.bodies[index];
      const std::uint32_t seed = fx::Hash(scene.seed + static_cast<std::uint32_t>(index) * 7919u);
      body.alt = index == 1;
      const float room = width - static_cast<float>(StandbyLogoWidth(body.alt));
      body.x = static_cast<float>(seed % static_cast<std::uint32_t>(room));
      body.y = index == 0 ? static_cast<float>((seed >> 8) % 24) : height - bodyHeight - static_cast<float>((seed >> 8) % 24);
      body.vx = (seed & 0x10000u) != 0 ? 15.0f : -15.0f;
      body.vy = index == 0 ? 10.0f : -10.0f;
      body.color = static_cast<std::uint8_t>((seed >> 20) % theme::kStandbyPalette.size());
      body.previousColor = body.color;
    }
    scene.bodies[1].color = static_cast<std::uint8_t>((scene.bodies[0].color + 4) % theme::kStandbyPalette.size());
    scene.bodies[1].previousColor = scene.bodies[1].color;
    scene.nextEffectAt = now_ + 12000 + (scene.seed >> 4) % 8000;
    return;
  }

  const float seconds = dt / 1000.0f;
  const std::uint32_t at = now_ == 0 ? 1 : now_;
  auto nextColor = [&](StandbyBody& body) {
    body.previousColor = body.color;
    body.color = static_cast<std::uint8_t>((body.color + 1) % theme::kStandbyPalette.size());
    body.colorAt = at;
  };
  for (StandbyBody& body : scene.bodies)
  {
    // After a hit each word eases back to its calm speed (keeping its heading), and never travels
    // straight along an axis.
    const float speed = std::hypot(body.vx, body.vy);
    if (speed > 0.01f)
    {
      const float ease = std::min(1.0f, seconds / 2.5f);
      const float scale = 1.0f + (kStandbyCruise / speed - 1.0f) * ease;
      body.vx *= scale;
      body.vy *= scale;
    }
    if (std::fabs(body.vx) < 6.0f) body.vx += (body.vx < 0.0f ? -1.0f : 1.0f) * 4.0f * seconds;
    if (std::fabs(body.vy) < 4.0f) body.vy += (body.vy < 0.0f ? -1.0f : 1.0f) * 3.0f * seconds;

    const float maxX = width - static_cast<float>(StandbyLogoWidth(body.alt));
    const float maxY = height - bodyHeight;
    body.x += body.vx * seconds;
    body.y += body.vy * seconds;
    bool hitX = false;
    bool hitY = false;
    if (body.x <= 0.0f || body.x >= maxX)
    {
      body.x = std::clamp(body.x <= 0.0f ? -body.x : 2.0f * maxX - body.x, 0.0f, maxX);
      body.vx = body.x < maxX * 0.5f ? std::fabs(body.vx) : -std::fabs(body.vx);
      hitX = true;
    }
    if (body.y <= 0.0f || body.y >= maxY)
    {
      body.y = std::clamp(body.y <= 0.0f ? -body.y : 2.0f * maxY - body.y, 0.0f, maxY);
      body.vy = body.y < maxY * 0.5f ? std::fabs(body.vy) : -std::fabs(body.vy);
      hitY = true;
    }
    // A wall hit with the other wall this close is the corner: the word is put right into it.
    constexpr float kCornerSlack = 4.0f;
    if (hitX != hitY)
    {
      float& other = hitX ? body.y : body.x;
      float& otherSpeed = hitX ? body.vy : body.vx;
      const float limit = hitX ? maxY : maxX;
      if (other <= kCornerSlack || other >= limit - kCornerSlack)
      {
        const bool far = other >= limit - kCornerSlack;
        other = far ? limit : 0.0f;
        otherSpeed = far ? -std::fabs(otherSpeed) : std::fabs(otherSpeed);
        hitX = hitY = true;
      }
    }
    if (hitX || hitY) nextColor(body);
    if (hitX && hitY)
    {
      scene.corneredAt = at;
      scene.cornerRight = body.x > maxX * 0.5f;
      scene.cornerBottom = body.y > maxY * 0.5f;
      scene.cornerColor = body.color;
      std::printf("ui: standby corner, %s\n", StandbyWord(body.alt));
    }
  }

  // The two words are solid to each other: overlapping boxes are pushed apart along the shallower side
  // and, when they were closing in, bounce off each other like two weights (the longer word heavier).
  StandbyBody& one = scene.bodies[0];
  StandbyBody& two = scene.bodies[1];
  const float oneWidth = static_cast<float>(StandbyLogoWidth(one.alt));
  const float twoWidth = static_cast<float>(StandbyLogoWidth(two.alt));
  const float overlapX = std::min(one.x + oneWidth, two.x + twoWidth) - std::max(one.x, two.x);
  const float overlapY = std::min(one.y + bodyHeight, two.y + bodyHeight) - std::max(one.y, two.y);
  if (overlapX > 0.0f && overlapY > 0.0f)
  {
    const bool alongX = overlapX < overlapY;
    float& p1 = alongX ? one.x : one.y;
    float& p2 = alongX ? two.x : two.y;
    float& v1 = alongX ? one.vx : one.vy;
    float& v2 = alongX ? two.vx : two.vy;
    const float c1 = p1 + (alongX ? oneWidth : bodyHeight) * 0.5f;
    const float c2 = p2 + (alongX ? twoWidth : bodyHeight) * 0.5f;
    const float direction = c1 < c2 ? 1.0f : -1.0f;  // from one towards two
    const float m1 = oneWidth;
    const float m2 = twoWidth;
    const float push = alongX ? overlapX : overlapY;
    p1 -= direction * push * m2 / (m1 + m2);
    p2 += direction * push * m1 / (m1 + m2);
    if ((v1 - v2) * direction > 0.0f)
    {
      const float u1 = v1;
      const float u2 = v2;
      v1 = (u1 * (m1 - m2) + 2.0f * m2 * u2) / (m1 + m2);
      v2 = (u2 * (m2 - m1) + 2.0f * m1 * u1) / (m1 + m2);
      scene.hitAt = at;
      scene.hitX = alongX ? (c1 < c2 ? one.x + oneWidth : two.x + twoWidth) : std::max(one.x, two.x) + overlapX * 0.5f;
      scene.hitY = alongX ? std::max(one.y, two.y) + overlapY * 0.5f : (c1 < c2 ? one.y + bodyHeight : two.y + bodyHeight);
      nextColor(one);
      nextColor(two);
    }
    // Pushed apart against a wall: back on screen, the other one gives way.
    for (StandbyBody* body : {&one, &two})
    {
      body->x = std::clamp(body->x, 0.0f, width - static_cast<float>(StandbyLogoWidth(body->alt)));
      body->y = std::clamp(body->y, 0.0f, height - bodyHeight);
    }
  }

  // Now and then an effect, every 20..36 s.
  if (static_cast<std::int32_t>(now_ - scene.nextEffectAt) >= 0)
  {
    const std::uint32_t noise = fx::Hash(now_ ^ scene.seed);
    scene.effect = static_cast<StandbyEffect>(1 + noise % kStandbyEffectCount);
    scene.effectAt = at;
    scene.nextEffectAt = now_ + 20000 + (noise >> 8) % 16000;
  }
}

namespace {

// The outer ring of keys, clockwise from the top left: top row, right column, bottom row, left column.
constexpr std::size_t kRingLength = 2 * kColumns + 2 * (kRows - 2);
// The inner ring: the second row left to right, the third right to left.
constexpr std::size_t kInnerLength = 2 * (kColumns - 2);

std::size_t RingKey(std::size_t position)
{
  position %= kRingLength;
  if (position < kColumns) return KeyIndex(0, position);
  position -= kColumns;
  if (position < kRows - 2) return KeyIndex(1 + position, kColumns - 1);
  position -= kRows - 2;
  if (position < kColumns) return KeyIndex(kRows - 1, kColumns - 1 - position);
  position -= kColumns;
  return KeyIndex(kRows - 2 - position, 0);
}

std::size_t InnerKey(std::size_t position)
{
  position %= kInnerLength;
  if (position < kColumns - 2) return KeyIndex(1, 1 + position);
  position -= kColumns - 2;
  return KeyIndex(2, kColumns - 2 - position);
}

// RING's path: twice round the outside, in to the middle and twice round it, back out and twice round
// the outside again, then from the start. Every step goes to a neighbouring key, so the light flows.
constexpr std::size_t kOuterIn = 2 * kRingLength;          // (0,0) round twice, ends at (1,0)
constexpr std::size_t kInnerLaps = 2 * kInnerLength;       // (1,1) round twice, ends at (2,1)
constexpr std::size_t kOuterOut = 2 * kRingLength + 2;     // from (2,0) round twice, ends at (1,0)
constexpr std::size_t kSpiralLength = kOuterIn + kInnerLaps + kOuterOut;

std::size_t SpiralKey(std::size_t step)
{
  step %= kSpiralLength;
  if (step < kOuterIn) return RingKey(step);
  step -= kOuterIn;
  if (step < kInnerLaps) return InnerKey(step);
  step -= kInnerLaps;
  return RingKey(kRingLength - 2 + step);
}

float SmoothStep(float t)
{
  t = fx::Clamp01(t);
  return t * t * (3.0f - 2.0f * t);
}

// One style of standby's light show, 0..15 per key, before it is dimmed. Every style moves slowly and
// lets each light rise and fall softly.
void StandbyStyle(StandbyLightStyle style, std::uint32_t t, std::uint32_t seed, std::array<float, kKeyCount>& levels)
{
  levels.fill(0.0f);
  const float seconds = static_cast<float>(t) / 1000.0f;
  switch (style)
  {
    case StandbyLightStyle::Ring:
    {
      // A comet with a long soft tail goes round the outside, spirals in and round the middle, and out
      // again (SpiralKey): three keys a second, about a minute for the whole way.
      const float head = seconds * 3.0f;
      const auto step = static_cast<std::size_t>(head);
      const float frac = head - static_cast<float>(step);
      // The key ahead brightens as the head comes; the ones behind fade along the path.
      levels[SpiralKey(step + 1)] = std::max(levels[SpiralKey(step + 1)], 15.0f * SmoothStep(frac));
      for (std::size_t behind = 0; behind < 9 && behind <= step; ++behind)
      {
        const float glow = std::exp(-(static_cast<float>(behind) + frac) / 2.4f);
        float& level = levels[SpiralKey(step - behind)];
        level = std::max(level, 15.0f * glow);
      }
      break;
    }
    case StandbyLightStyle::Sweep:
    {
      // A wide, slanted band of light drifts from left to right, then comes round again from off the left.
      const float span = static_cast<float>(kColumns) + 12.0f;
      const float band = std::fmod(seconds * 1.8f, span) - 6.0f;
      for (std::size_t row = 0; row < kRows; ++row)
        for (std::size_t column = 0; column < kColumns; ++column)
        {
          const float distance = static_cast<float>(column) - (band - static_cast<float>(row) * 0.7f);
          levels[KeyIndex(row, column)] = 15.0f * std::exp(-distance * distance / 3.0f);
        }
      break;
    }
    case StandbyLightStyle::Rain:
    {
      // Drops fall slowly down the columns, top to bottom, each column on its own time: a soft head
      // that eases in above the top row and a tail that fades.
      for (std::size_t column = 0; column < kColumns; ++column)
      {
        const std::uint32_t noise = fx::Hash(seed + static_cast<std::uint32_t>(column) * 7919u);
        const std::uint32_t period = 4500 + noise % 5000;
        const std::uint32_t local = (t + (noise >> 12) % period) % period;
        const float head = static_cast<float>(local) / 600.0f - 1.0f;  // rows, starting above the top
        for (std::size_t row = 0; row < kRows; ++row)
        {
          const float behind = head - static_cast<float>(row);
          float level = 0.0f;
          if (behind >= -1.0f && behind < 0.0f) level = 15.0f * SmoothStep(behind + 1.0f);
          else if (behind >= 0.0f) level = 15.0f * std::exp(-behind / 0.9f);
          levels[KeyIndex(row, column)] = level;
        }
      }
      break;
    }
    case StandbyLightStyle::Aurora:
    {
      // Soft light drifting over the keyboard like an aurora: two slow waves, only their crests glow.
      for (std::size_t row = 0; row < kRows; ++row)
        for (std::size_t column = 0; column < kColumns; ++column)
        {
          const float x = static_cast<float>(column);
          const float y = static_cast<float>(row);
          const float a = std::sin(x * 0.42f - seconds * 0.34f + std::sin(y * 0.9f + seconds * 0.19f) * 1.2f);
          const float b = std::sin(x * 0.23f + y * 0.7f + seconds * 0.23f);
          const float crest = std::clamp((a + b) * 0.5f, 0.0f, 1.0f);
          levels[KeyIndex(row, column)] = 15.0f * crest * crest;
        }
      break;
    }
    case StandbyLightStyle::Stars:
    {
      // Keys twinkle here and there: each rises and falls softly, on its own slow time.
      for (std::size_t index = 0; index < kKeyCount; ++index)
      {
        const std::uint32_t noise = fx::Hash(seed ^ (static_cast<std::uint32_t>(index) * 2246822519u));
        const std::uint32_t period = 6000 + noise % 7000;
        const float phase = static_cast<float>((t + (noise >> 11) % period) % period) / static_cast<float>(period);
        if (phase < 0.36f)
        {
          const float glow = std::sin(3.14159265f * phase / 0.36f);
          levels[index] = 15.0f * glow * glow;
        }
      }
      break;
    }
    case StandbyLightStyle::All:
    case StandbyLightStyle::Off:
      break;
  }
}

}  // namespace

// Standby's key-light shows: the whole keyboard, edge keys too, running free (the user asked for it;
// everywhere else the lights only say state). They fade in, stay below full brightness, and ALL lets
// each style fade into the next.
void Animator::StandbyLeds(const Controller& controller, std::array<std::uint8_t, kKeyCount>& levels) const
{
  levels.fill(0);
  const std::uint32_t t = now_ - controller.StandbyAt();
  const auto chosen = static_cast<StandbyLightStyle>(std::min<std::size_t>(controller.HostView().StandbyLights(), kStandbyLightCount - 1));
  std::array<float, kKeyCount> shown{};
  if (chosen == StandbyLightStyle::All)
  {
    static constexpr std::array<StandbyLightStyle, 5> kTurns{{StandbyLightStyle::Ring, StandbyLightStyle::Aurora, StandbyLightStyle::Sweep,
                                                             StandbyLightStyle::Stars, StandbyLightStyle::Rain}};
    const std::size_t turn = (t / kStandbyStyleMs) % kTurns.size();
    StandbyStyle(kTurns[turn], t, standby_.seed, shown);
    const std::uint32_t into = t % kStandbyStyleMs;
    if (into + kStandbyBlendMs > kStandbyStyleMs)
    {
      std::array<float, kKeyCount> next{};
      StandbyStyle(kTurns[(turn + 1) % kTurns.size()], t, standby_.seed, next);
      const float blend = SmoothStep(static_cast<float>(into + kStandbyBlendMs - kStandbyStyleMs) / static_cast<float>(kStandbyBlendMs));
      for (std::size_t index = 0; index < kKeyCount; ++index) shown[index] += (next[index] - shown[index]) * blend;
    }
  }
  else StandbyStyle(chosen, t, standby_.seed, shown);
  const float in = SmoothStep(static_cast<float>(t) / static_cast<float>(kStandbyLightsInMs));
  for (std::size_t index = 0; index < kKeyCount; ++index)
    levels[index] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(shown[index] * kStandbyLedScale * in)), 0, 15));
}

void Animator::SmoothLeds(std::array<std::uint8_t, kKeyCount>& levels)
{
  const float dt = ledFrame_ == 0 ? 1000.0f : static_cast<float>(std::min<std::uint32_t>(now_ - ledFrame_, 200));
  ledFrame_ = now_ == 0 ? 1 : now_;
  const float rise = 1.0f - std::exp(-dt / 22.0f);
  const float fall = 1.0f - std::exp(-dt / 60.0f);
  for (std::size_t index = 0; index < kKeyCount; ++index)
  {
    const float target = levels[index];
    float& shown = ledShown_[index];
    shown += (target - shown) * (target > shown ? (ledStruck_[index] ? 1.0f : rise) : fall);
    if (std::fabs(target - shown) < 0.3f) shown = target;
    levels[index] = static_cast<std::uint8_t>(std::clamp(static_cast<int>(std::lround(shown)), 0, 15));
  }
}

}  // namespace groove
