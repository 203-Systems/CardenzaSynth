#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <new>
#include <string>
#include <utility>

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

namespace synth {

namespace {

constexpr std::array<std::uint16_t, kHomePerformanceColumnCount> kLengthBuckets = {
    kSequencerPulsesPerStep,
    kSequencerPulsesPerStep * 2,
    kSequencerPulsesPerStep * 3,
    kSequencerPulsesPerStep * 4,
    kSequencerPulsesPerStep * 6,
    kSequencerPulsesPerStep * 8,
    kSequencerPulsesPerStep * 12,
    kSequencerPulsesPerStep * 16,
};

constexpr std::array<std::uint8_t, kHomePerformanceColumnCount> kVelocityBuckets = {
    24, 40, 56, 72, 88, 104, 120, 127,
};

constexpr std::size_t kClockIdleSleepMs = 2;
constexpr std::size_t kClockTaskStackWords = 4096;
constexpr int kClockTaskPriority = 4;

}  // namespace

namespace {

const SynthSequencer::PatternData kEmptyPattern{};
SynthSequencer::PatternData scratchPattern{};

}  // namespace

SynthSequencer::PatternBank& SynthSequencer::PatternBank::operator=(const PatternBank& other)
{
  if (this != &other) (void)CopyFrom(other);
  return *this;
}

SynthSequencer::PatternData& SynthSequencer::PatternBank::operator[](std::size_t index)
{
  if (Ensure(index)) return *patterns_[index];
  scratchPattern = PatternData{};
  return scratchPattern;
}

const SynthSequencer::PatternData& SynthSequencer::PatternBank::operator[](std::size_t index) const
{
  return Allocated(index) ? *patterns_[index] : kEmptyPattern;
}

bool SynthSequencer::PatternBank::Allocated(std::size_t index) const
{
  return index < patterns_.size() && patterns_[index] != nullptr;
}

bool SynthSequencer::PatternBank::Ensure(std::size_t index)
{
  if (index >= patterns_.size()) return false;
  if (!patterns_[index]) patterns_[index].reset(new (std::nothrow) PatternData);
  return patterns_[index] != nullptr;
}

bool SynthSequencer::PatternBank::Reserve(const PatternBank& other)
{
  for (std::size_t index = 0; index < patterns_.size(); ++index)
    if (other.Allocated(index) && !Ensure(index)) return false;
  return true;
}

bool SynthSequencer::PatternBank::CopyFrom(const PatternBank& other)
{
  if (this == &other) return true;
  // A failed reserve only adds empty patterns, so the contents stay unchanged.
  if (!Reserve(other)) return false;
  for (std::size_t index = 0; index < patterns_.size(); ++index)
  {
    if (other.Allocated(index)) *patterns_[index] = *other.patterns_[index];
    else patterns_[index].reset();
  }
  return true;
}

void SynthSequencer::PatternBank::Release(std::size_t index)
{
  if (index < patterns_.size()) patterns_[index].reset();
}

void SynthSequencer::PatternBank::Swap(std::size_t left, std::size_t right)
{
  if (left < patterns_.size() && right < patterns_.size()) patterns_[left].swap(patterns_[right]);
}

void SynthSequencer::PatternBank::Clear()
{
  for (auto& pattern : patterns_) pattern.reset();
}

SynthSequencer::SynthSequencer() = default;

SynthSequencer::~SynthSequencer()
{
  Stop();
  StopClockWorker();
}

void SynthSequencer::SetMidiSink(MidiSink sink, void* context)
{
  std::lock_guard<std::mutex> lock(mutex_);
  midiSink_ = sink;
  midiSinkContext_ = context;
}

void SynthSequencer::SetActiveTrack(std::size_t trackIndex)
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (trackIndex < tracks_.size())
    {
      if (trackIndex != activeTrack_)
      {
        StopAuditionLocked(events, eventCount);
        heldSteps_.fill(false);
        heldEditLanes_.fill(false);
        editLane_ = SequencerEditLane::Note;
        noteScopeIndex_ = -1;
      }
      activeTrack_ = trackIndex;
      NormalizeScopeLocked();
    }
  }
  EmitPendingMidi(events, eventCount);
}

void SynthSequencer::Play()
{
  EnsureClockWorkerStarted();
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::lock_guard<std::mutex> lock(mutex_);
  if (playing_ || !clockWorkerRunning_.load())
  {
    return;
  }
  absolutePulse_ = 0;
  playbackStep_ = 0;
  playing_ = true;
}

void SynthSequencer::Stop()
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    playing_ = false;
    CollectAllNoteOffsLocked(events, eventCount);
  }
  EmitPendingMidi(events, eventCount);
}

void SynthSequencer::TogglePlayback()
{
  if (Playing())
  {
    Stop();
    return;
  }
  Play();
}

bool SynthSequencer::Playing() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return playing_;
}

void SynthSequencer::Resume()
{
  EnsureClockWorkerStarted();
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::lock_guard<std::mutex> lock(mutex_);
  playing_ = clockWorkerRunning_.load();
}

void SynthSequencer::AdjustTempo(int delta)
{
  std::lock_guard<std::mutex> lock(mutex_);
  bpm_ = static_cast<std::uint16_t>(std::clamp(static_cast<int>(bpm_) + delta, 30, 300));
}

void SynthSequencer::ToggleTrackMuted()
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    TrackData& track = tracks_[activeTrack_];
    track.muted = !track.muted;
    if (track.muted) {
      for (PlaybackNote& note : playbackNotes_) {
        if (note.active && note.trackIndex == activeTrack_) {
          AddPendingMidi(events, count, note.trackIndex, midi::MidiPacket::NoteOff(0, note.note, 0));
          note = PlaybackNote{};
        }
      }
    }
  }
  EmitPendingMidi(events, count);
}

void SynthSequencer::ToggleTrackEnabled()
{
  // Reuse mute's note release without changing the stored mute preference.
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t count = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    TrackData& track = tracks_[activeTrack_];
    track.enabled = !track.enabled;
    if (!track.enabled) {
      for (PlaybackNote& note : playbackNotes_) {
        if (note.active && note.trackIndex == activeTrack_) {
          AddPendingMidi(events, count, note.trackIndex, midi::MidiPacket::NoteOff(0, note.note, 0));
          note = PlaybackNote{};
        }
      }
    }
  }
  EmitPendingMidi(events, count);
}

void SynthSequencer::Reset()
{
  Stop();
  ClearHeldSteps();
  std::lock_guard<std::mutex> lock(mutex_);
  // Clear one pattern at a time; a temporary full ProjectState would overflow a task stack.
  for (TrackData& track : tracks_) {
    track.patterns.Clear();
    track.patternCount = 1;
    track.activePattern = 0;
    track.defaultVelocity = 110;
    track.defaultLengthPulses = kSequencerPulsesPerStep;
    track.enabled = true;
    track.muted = false;
  }
  bpm_ = 120;
  absolutePulse_ = 0;
  playbackStep_ = 0;
}

std::unique_ptr<SynthSequencer::ProjectState> SynthSequencer::CaptureState() const
{
  auto state = std::unique_ptr<ProjectState>(new (std::nothrow) ProjectState);
  if (!state) return nullptr;
  std::lock_guard<std::mutex> lock(mutex_);
  // Reserve first so the copy below cannot silently drop patterns when memory runs out.
  for (std::size_t index = 0; index < tracks_.size(); ++index)
    if (!state->tracks[index].patterns.Reserve(tracks_[index].patterns)) return nullptr;
  state->bpm = bpm_;
  state->tracks = tracks_;
  return state;
}

bool SynthSequencer::ValidateState(const ProjectState& state)
{
  if (state.bpm < 30 || state.bpm > 300) return false;
  for (const TrackData& track : state.tracks) {
    if (track.patternCount < 1 || track.patternCount > kSequencerMaxPatternCount ||
        track.activePattern >= track.patternCount || track.defaultVelocity == 0 || track.defaultVelocity > 127 ||
        track.defaultLengthPulses == 0 || track.defaultLengthPulses > kSequencerStepCount * kSequencerPulsesPerStep) return false;
    for (std::size_t patternIndex = 0; patternIndex < track.patterns.size(); ++patternIndex) {
      if (!track.patterns.Allocated(patternIndex)) continue;
      const PatternData& pattern = track.patterns[patternIndex];
      for (std::size_t stepIndex = 0; stepIndex < pattern.steps.size(); ++stepIndex) {
        std::array<bool, 128> notes{};
        for (const NoteEvent& note : pattern.steps[stepIndex].notes) {
          if (!note.active) continue;
          if (note.note > 127 || note.velocity == 0 || note.velocity > 127 || note.microstep >= kSequencerMicrostepCount ||
              note.lengthPulses == 0 || note.lengthPulses > (kSequencerStepCount - stepIndex) * kSequencerPulsesPerStep - note.microstep * kSequencerPulsesPerMicrostep ||
              notes[note.note]) return false;
          notes[note.note] = true;
        }
      }
    }
  }
  return true;
}

bool SynthSequencer::RestoreState(const ProjectState& state)
{
  if (!ValidateState(state)) return false;
  Stop();
  ClearHeldSteps();
  std::lock_guard<std::mutex> lock(mutex_);
  for (std::size_t index = 0; index < tracks_.size(); ++index)
    if (!tracks_[index].patterns.Reserve(state.tracks[index].patterns)) return false;
  tracks_ = state.tracks;
  bpm_ = state.bpm;
  absolutePulse_ = 0;
  playbackStep_ = 0;
  return true;
}

void SynthSequencer::StepPattern(int delta)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || delta == 0)
  {
    return;
  }

  TrackData& track = tracks_[activeTrack_];
  track.patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  const int count = static_cast<int>(track.patternCount);
  const int next = (static_cast<int>(track.activePattern) + delta % count + count) % count;
  track.activePattern = static_cast<std::size_t>(next);
  NormalizeScopeLocked();
}

void SynthSequencer::AdjustPatternCount(int delta)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || delta == 0)
  {
    return;
  }

  TrackData& track = tracks_[activeTrack_];
  const int current = static_cast<int>(std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount));
  const int next = std::clamp(current + delta, 1, static_cast<int>(kSequencerMaxPatternCount));
  for (int index = next; index < current; ++index)
  {
    track.patterns.Release(static_cast<std::size_t>(index));
  }
  track.patternCount = static_cast<std::size_t>(next);
  if (track.activePattern >= track.patternCount)
  {
    track.activePattern = track.patternCount - 1;
  }
  NormalizeScopeLocked();
}

bool SynthSequencer::SelectOrCreatePattern(std::size_t patternIndex)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || patternIndex >= kSequencerMaxPatternCount)
  {
    return false;
  }

  TrackData& track = tracks_[activeTrack_];
  track.patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  if (patternIndex > track.patternCount)
  {
    return false;
  }

  if (patternIndex == track.patternCount)
  {
    if (track.patternCount >= kSequencerMaxPatternCount)
    {
      return false;
    }
    ++track.patternCount;
  }

  track.activePattern = patternIndex;
  NormalizeScopeLocked();
  return true;
}

bool SynthSequencer::DeletePattern(std::size_t patternIndex)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || patternIndex >= kSequencerMaxPatternCount)
  {
    return false;
  }

  TrackData& track = tracks_[activeTrack_];
  track.patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  if (patternIndex >= track.patternCount)
  {
    return false;
  }

  if (track.patternCount == 1)
  {
    track.patterns.Release(0);
    track.activePattern = 0;
    NormalizeScopeLocked();
    return true;
  }

  for (std::size_t index = patternIndex; index + 1 < track.patternCount; ++index)
  {
    track.patterns.Swap(index, index + 1);
  }
  track.patterns.Release(track.patternCount - 1);
  --track.patternCount;
  if (track.activePattern > patternIndex) --track.activePattern;
  track.activePattern = std::min(track.activePattern, track.patternCount - 1);
  NormalizeScopeLocked();
  return true;
}

bool SynthSequencer::CopyPattern(std::size_t sourcePattern, std::size_t destinationPattern)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || sourcePattern >= kSequencerMaxPatternCount || destinationPattern >= kSequencerMaxPatternCount)
  {
    return false;
  }

  TrackData& track = tracks_[activeTrack_];
  track.patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  if (sourcePattern >= track.patternCount || destinationPattern > track.patternCount || !track.patterns.Ensure(destinationPattern))
  {
    return false;
  }

  if (destinationPattern == track.patternCount)
  {
    if (track.patternCount >= kSequencerMaxPatternCount)
    {
      return false;
    }
    ++track.patternCount;
  }

  track.patterns[destinationPattern] = std::as_const(track.patterns)[sourcePattern];
  track.activePattern = destinationPattern;
  NormalizeScopeLocked();
  return true;
}

bool SynthSequencer::PatternHasEvents(std::size_t patternIndex) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || patternIndex >= kSequencerMaxPatternCount)
  {
    return false;
  }

  const TrackData& track = tracks_[activeTrack_];
  const std::size_t patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  if (patternIndex >= patternCount)
  {
    return false;
  }

  for (const StepData& step : track.patterns[patternIndex].steps)
  {
    if (ActiveNoteCount(step) > 0)
    {
      return true;
    }
  }
  return false;
}

void SynthSequencer::PressStep(std::size_t stepIndex, const std::uint8_t* heldNotes, std::size_t heldNoteCount)
{
  if (stepIndex >= kSequencerStepCount)
  {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  heldSteps_[stepIndex] = true;
  if (activeTrack_ >= tracks_.size() || heldNotes == nullptr || heldNoteCount == 0 || editLane_ != SequencerEditLane::Note)
  {
    NormalizeScopeLocked();
    return;
  }

  TrackData& track = tracks_[activeTrack_];
  StepData& step = ActivePatternLocked(track).steps[stepIndex];
  std::array<bool, 128> seen{};
  for (std::size_t index = 0; index < heldNoteCount; ++index)
  {
    const std::uint8_t note = heldNotes[index];
    if (note < seen.size() && !seen[note])
    {
      seen[note] = true;
      ToggleNoteInStepLocked(step, note, track, stepIndex);
    }
  }
  NormalizeScopeLocked();
}

void SynthSequencer::ReleaseStep(std::size_t stepIndex)
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  if (stepIndex >= kSequencerStepCount)
  {
    return;
  }

  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    heldSteps_[stepIndex] = false;
    if (!std::any_of(heldSteps_.begin(), heldSteps_.end(), [](bool held) { return held; }))
    {
      StopAuditionLocked(events, eventCount);
      heldEditLanes_.fill(false);
      editLane_ = SequencerEditLane::Note;
      noteScopeIndex_ = -1;
    }
    else
    {
      NormalizeScopeLocked();
    }
  }
  EmitPendingMidi(events, eventCount);
}

void SynthSequencer::ClearHeldSteps()
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    StopAuditionLocked(events, eventCount);
    heldSteps_.fill(false);
    heldEditLanes_.fill(false);
    editLane_ = SequencerEditLane::Note;
    noteScopeIndex_ = -1;
  }
  EmitPendingMidi(events, eventCount);
}

void SynthSequencer::ClearStep(std::size_t stepIndex)
{
  if (stepIndex >= kSequencerStepCount)
  {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  ClearStepLocked(ActivePatternLocked(tracks_[activeTrack_]).steps[stepIndex]);
  NormalizeScopeLocked();
}

void SynthSequencer::ClearHeldStepEvents()
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (activeTrack_ >= tracks_.size())
    {
      return;
    }

    StopAuditionLocked(events, eventCount);
    PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
    for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
    {
      if (heldSteps_[stepIndex])
      {
        ClearStepLocked(pattern.steps[stepIndex]);
      }
    }
    NormalizeScopeLocked();
  }
  EmitPendingMidi(events, eventCount);
}

bool SynthSequencer::CopyStep(std::size_t sourceStep, std::size_t destinationStep)
{
  if (sourceStep >= kSequencerStepCount || destinationStep >= kSequencerStepCount)
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return false;
  }

  PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
  if (ActiveNoteCount(pattern.steps[sourceStep]) == 0)
  {
    return false;
  }

  pattern.steps[destinationStep] = pattern.steps[sourceStep];
  for (NoteEvent& note : pattern.steps[destinationStep].notes)
  {
    if (note.active)
      note.lengthPulses = ClampLengthForStepMicrostep(destinationStep, note.microstep, note.lengthPulses);
  }
  NormalizeScopeLocked();
  return true;
}

bool SynthSequencer::StepHasEvents(std::size_t stepIndex) const
{
  if (stepIndex >= kSequencerStepCount)
  {
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return false;
  }

  return ActiveNoteCount(ActivePatternLocked(tracks_[activeTrack_]).steps[stepIndex]) > 0;
}

void SynthSequencer::QuantizeStep(std::size_t stepIndex)
{
  if (stepIndex >= kSequencerStepCount)
  {
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  QuantizeStepLocked(ActivePatternLocked(tracks_[activeTrack_]).steps[stepIndex], stepIndex);
  NormalizeScopeLocked();
}

void SynthSequencer::QuantizeHeldSteps()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
  for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
  {
    if (heldSteps_[stepIndex])
    {
      QuantizeStepLocked(pattern.steps[stepIndex], stepIndex);
    }
  }
  NormalizeScopeLocked();
}

void SynthSequencer::QuantizeActivePattern()
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
  for (std::size_t stepIndex = 0; stepIndex < pattern.steps.size(); ++stepIndex)
  {
    QuantizeStepLocked(pattern.steps[stepIndex], stepIndex);
  }
  NormalizeScopeLocked();
}

void SynthSequencer::QuantizePattern(std::size_t patternIndex)
{
  std::lock_guard<std::mutex> lock(mutex_);
  if (activeTrack_ >= tracks_.size() || patternIndex >= tracks_[activeTrack_].patternCount ||
      patternIndex >= kSequencerMaxPatternCount)
  {
    return;
  }

  PatternData& pattern = tracks_[activeTrack_].patterns[patternIndex];
  for (std::size_t stepIndex = 0; stepIndex < pattern.steps.size(); ++stepIndex)
  {
    QuantizeStepLocked(pattern.steps[stepIndex], stepIndex);
  }
  NormalizeScopeLocked();
}

bool SynthSequencer::IsStepHeld(std::size_t stepIndex) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return stepIndex < heldSteps_.size() && heldSteps_[stepIndex];
}

bool SynthSequencer::HasHeldSteps() const
{
  std::lock_guard<std::mutex> lock(mutex_);
  return std::any_of(heldSteps_.begin(), heldSteps_.end(), [](bool held) { return held; });
}

bool SynthSequencer::PressEditLane(std::size_t row)
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  if (row >= kKeyboardMatrixRows)
  {
    return false;
  }

  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!std::any_of(heldSteps_.begin(), heldSteps_.end(), [](bool held) { return held; }))
    {
      return false;
    }

    StopAuditionLocked(events, eventCount);
    heldEditLanes_[row] = true;
    switch (row)
    {
      case 0:
        editLane_ = SequencerEditLane::Note;
        CycleNoteScopeLocked();
        AuditionScopedNoteLocked(events, eventCount);
        break;
      case 1:
        editLane_ = SequencerEditLane::Length;
        NormalizeScopeLocked();
        break;
      case 2:
        editLane_ = SequencerEditLane::Velocity;
        NormalizeScopeLocked();
        break;
      case 3:
        editLane_ = SequencerEditLane::Micro;
        NormalizeScopeLocked();
        break;
      default:
        return false;
    }
  }
  EmitPendingMidi(events, eventCount);
  return true;
}

bool SynthSequencer::ReleaseEditLane(std::size_t row)
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  if (row >= kKeyboardMatrixRows)
  {
    return false;
  }

  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    heldEditLanes_[row] = false;
    StopAuditionLocked(events, eventCount);
  }
  EmitPendingMidi(events, eventCount);
  return true;
}

bool SynthSequencer::HandlePerformanceKeyPress(std::size_t, std::size_t column, std::uint8_t note)
{
  if (column < kHomePerformanceFirstColumn || column > kHomePerformanceLastColumn)
  {
    return false;
  }

  const std::size_t bucket = column - kHomePerformanceFirstColumn;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!std::any_of(heldSteps_.begin(), heldSteps_.end(), [](bool held) { return held; }) || activeTrack_ >= tracks_.size())
  {
    return false;
  }

  if (editLane_ == SequencerEditLane::Note)
  {
    TrackData& track = tracks_[activeTrack_];
    PatternData& pattern = ActivePatternLocked(track);
    for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
    {
      if (heldSteps_[stepIndex])
      {
        ToggleNoteInStepLocked(pattern.steps[stepIndex], note, track, stepIndex);
      }
    }
    NormalizeScopeLocked();
    return true;
  }

  if (editLane_ == SequencerEditLane::Length)
  {
    ApplyLengthToHeldStepsLocked(kLengthBuckets[bucket]);
    return true;
  }

  if (editLane_ == SequencerEditLane::Velocity)
  {
    ApplyVelocityToHeldStepsLocked(kVelocityBuckets[bucket]);
    return true;
  }

  if (editLane_ == SequencerEditLane::Micro)
  {
    if (bucket < kSequencerMicrostepCount)
    {
      ApplyMicrostepToHeldStepsLocked(static_cast<std::uint8_t>(bucket));
    }
    return true;
  }

  return false;
}

SequencerUiState SynthSequencer::BuildUiState(std::size_t activeTrack) const
{
  std::lock_guard<std::mutex> lock(mutex_);
  SequencerUiState state;
  state.visible = true;
  state.playing = playing_;
  state.bpm = bpm_;
  state.currentStep = playbackStep_;
  state.patternProgress = playing_ ? static_cast<float>(absolutePulse_ % (kSequencerStepCount * kSequencerPulsesPerStep)) /
                                         static_cast<float>(kSequencerStepCount * kSequencerPulsesPerStep)
                                   : 0.0f;
  state.editLane = editLane_;
  state.editLanePressed = heldEditLanes_;
  state.heldStepCount = static_cast<std::size_t>(std::count(heldSteps_.begin(), heldSteps_.end(), true));
  state.hasHeldSteps = state.heldStepCount != 0;
  for (std::size_t index = 0; index < tracks_.size(); ++index)
  {
    const auto& source = tracks_[index];
    auto& summary = state.tracks[index];
    summary.enabled = source.enabled;
    summary.muted = source.muted;
    summary.activePattern = source.activePattern;
    summary.patternCount = source.patternCount;
    for (const StepData& step : ActivePatternLocked(source).steps)
      if (ActiveNoteCount(step) != 0) ++summary.populatedStepCount;
  }
  const TrackData* track = activeTrack < tracks_.size() ? &tracks_[activeTrack] : nullptr;
  if (track != nullptr)
  {
    state.trackEnabled = track->enabled;
    state.trackMuted = track->muted;
    state.patternCount = std::clamp<std::size_t>(track->patternCount, 1, kSequencerMaxPatternCount);
    state.activePattern = std::min(track->activePattern, state.patternCount - 1);
    state.populatedStepCount = state.tracks[activeTrack].populatedStepCount;
    for (std::size_t index = 0; index < state.patternCount; ++index)
      for (const StepData& step : track->patterns[index].steps)
        if (ActiveNoteCount(step) != 0) { state.patternHasEvents[index] = true; break; }
  }
  const StepData* primaryHeldStep = nullptr;
  for (std::size_t stepIndex = 0; stepIndex < state.steps.size(); ++stepIndex)
  {
    const StepData* step = track != nullptr ? &ActivePatternLocked(*track).steps[stepIndex] : nullptr;
    auto& cell = state.steps[stepIndex];
    if (step != nullptr)
      for (const NoteEvent& note : step->notes)
        if (note.active)
        {
          ++cell.noteCount;
          if (cell.lowestNote < 0 || note.note < cell.lowestNote) cell.lowestNote = note.note;
        }
    cell.hasEvents = cell.noteCount > 0;
    state.steps[stepIndex].held = heldSteps_[stepIndex];
    state.steps[stepIndex].playing = playing_ && playbackStep_ == stepIndex;
    if (heldSteps_[stepIndex] && primaryHeldStep == nullptr)
    {
      primaryHeldStep = step;
      state.primaryHeldStep = stepIndex;
    }
  }
  if (primaryHeldStep != nullptr)
  {
    state.noteCount = ActiveNoteCount(*primaryHeldStep);
    // A 128-note presence table gives stable pitch order without allocating a chord copy.
    std::array<bool, 128> pitches{};
    for (const NoteEvent& note : primaryHeldStep->notes)
      if (note.active && note.note < pitches.size()) pitches[note.note] = true;
    static constexpr std::array<const char*, 12> names{{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"}};
    for (std::size_t pitch = 0; pitch < pitches.size(); ++pitch)
    {
      if (!pitches[pitch]) continue;
      if (!state.noteNames.empty()) state.noteNames += ' ';
      state.noteNames += names[pitch % names.size()];
      state.noteNames += std::to_string(static_cast<int>(pitch / names.size()) - 1);
    }
    const std::size_t slot = ScopedNoteSlot(*primaryHeldStep);
    if (slot < primaryHeldStep->notes.size())
    {
      state.scopedNote = primaryHeldStep->notes[slot].note;
      for (std::size_t stepIndex = 0; stepIndex < state.steps.size(); ++stepIndex)
      {
        if (heldSteps_[stepIndex])
        {
          state.steps[stepIndex].scoped = true;
        }
      }
    }
  }
  state.scopeLabel = ScopeLabelLocked(primaryHeldStep);
  state.valueLabel = ValueLabelLocked(primaryHeldStep);
  PopulateEditValueStateLocked(state, primaryHeldStep);
  if (track != nullptr && editLane_ != SequencerEditLane::Note)
  {
    int firstValue = -1;
    for (std::size_t index = 0; index < heldSteps_.size(); ++index)
    {
      if (!heldSteps_[index]) continue;
      const StepData& step = ActivePatternLocked(*track).steps[index];
      const std::size_t scopedSlot = ScopedNoteSlot(step);
      for (std::size_t slot = 0; slot < step.notes.size(); ++slot)
      {
        const NoteEvent& note = step.notes[slot];
        if (!note.active || (noteScopeIndex_ >= 0 && slot != scopedSlot)) continue;
        const int value = editLane_ == SequencerEditLane::Length ? note.lengthPulses :
            (editLane_ == SequencerEditLane::Velocity ? note.velocity : note.microstep);
        if (firstValue < 0) firstValue = value;
        else if (value != firstValue) state.editValueMixed = true;
      }
    }
    if (state.editValueMixed) state.valueLabel = std::string(SequencerEditLaneLabel(editLane_)) + " MIXED";
  }
  return state;
}

void SynthSequencer::EnsureClockWorkerStarted()
{
  bool expected = false;
  if (!clockWorkerRunning_.compare_exchange_strong(expected, true))
  {
    return;
  }

  clockWorkerExited_.store(false);

#if defined(ESP_PLATFORM)
  TaskHandle_t taskHandle = nullptr;
  if (xTaskCreate(&SynthSequencer::ClockTaskEntry,
                  "synth_seq",
                  kClockTaskStackWords,
                  this,
                  kClockTaskPriority,
                  &taskHandle) != pdPASS)
  {
    clockWorkerRunning_.store(false);
    clockWorkerExited_.store(true);
    return;
  }
#else
  clockThread_ = std::thread([this]() { ClockLoop(); });
#endif
}

void SynthSequencer::StopClockWorker()
{
  clockWorkerRunning_.store(false);
#if defined(ESP_PLATFORM)
  while (!clockWorkerExited_.load()) vTaskDelay(1);
#else
  if (clockThread_.joinable())
  {
    clockThread_.join();
  }
#endif
}

#if defined(ESP_PLATFORM)
void SynthSequencer::ClockTaskEntry(void* context)
{
  static_cast<SynthSequencer*>(context)->ClockLoop();
  vTaskDelete(nullptr);
}
#endif

void SynthSequencer::ClockLoop()
{
#if defined(ESP_PLATFORM)
  esp_timer_create_args_t timerArgs{};
  timerArgs.callback = [](void* task) { xTaskNotifyGive(static_cast<TaskHandle_t>(task)); };
  timerArgs.arg = xTaskGetCurrentTaskHandle();
  timerArgs.name = "seq_pulse";
  esp_timer_handle_t timer = nullptr;
  if (esp_timer_create(&timerArgs, &timer) != ESP_OK) {
    clockWorkerRunning_.store(false);
    { std::lock_guard<std::mutex> lock(mutex_); playing_ = false; }
    clockWorkerExited_.store(true);
    return;
  }
  clockTimer_ = timer;
#endif
  std::uint64_t nextPulseMicros = NowMicros();
  while (clockWorkerRunning_.load())
  {
    if (!Playing())
    {
#if defined(ESP_PLATFORM)
      vTaskDelay(std::max<TickType_t>(1, pdMS_TO_TICKS(kClockIdleSleepMs)));
#else
      std::this_thread::sleep_for(std::chrono::milliseconds(kClockIdleSleepMs));
#endif
      nextPulseMicros = NowMicros();
      continue;
    }

    const std::uint64_t now = NowMicros();
    if (now < nextPulseMicros)
    {
      SleepUntilMicros(nextPulseMicros);
      continue;
    }

    ProcessPulse();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      nextPulseMicros += PulsePeriodMicrosLocked();
    }

    const std::uint64_t after = NowMicros();
    if (after > nextPulseMicros + 50000)
    {
      nextPulseMicros = after;
    }
  }
#if defined(ESP_PLATFORM)
  esp_timer_stop(timer);
  esp_timer_delete(timer);
  clockTimer_ = nullptr;
#endif
  clockWorkerExited_.store(true);
}

void SynthSequencer::ProcessPulse()
{
  std::lock_guard<std::mutex> outputLock(outputMutex_);
  std::array<PendingMidiEvent, kMaxPendingMidiEvents> events{};
  std::size_t eventCount = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!playing_)
    {
      return;
    }

    const std::uint64_t pulse = absolutePulse_;
    const std::size_t stepIndex = static_cast<std::size_t>((pulse / kSequencerPulsesPerStep) % kSequencerStepCount);
    const std::uint16_t pulseInStep = static_cast<std::uint16_t>(pulse % kSequencerPulsesPerStep);
    playbackStep_ = stepIndex;

    CollectDueNoteOffsLocked(pulse, events, eventCount);

    for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex)
    {
      const TrackData& track = tracks_[trackIndex];
      if (!track.enabled || track.muted)
      {
        continue;
      }

      const StepData& step = ActivePatternLocked(track).steps[stepIndex];
      for (const NoteEvent& note : step.notes)
      {
        if (!note.active || static_cast<std::uint16_t>(note.microstep) * kSequencerPulsesPerMicrostep != pulseInStep)
        {
          continue;
        }

        if (ScheduleNoteOffLocked(trackIndex, note.note, pulse + std::max<std::uint16_t>(1, note.lengthPulses)))
          AddPendingMidi(events, eventCount, trackIndex, midi::MidiPacket::NoteOn(0, note.note, note.velocity));
      }
    }

    ++absolutePulse_;
  }
  EmitPendingMidi(events, eventCount);
}

std::uint64_t SynthSequencer::NowMicros() const
{
#if defined(ESP_PLATFORM)
  return static_cast<std::uint64_t>(esp_timer_get_time());
#else
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(now).count());
#endif
}

void SynthSequencer::SleepUntilMicros(std::uint64_t targetMicros) const
{
#if defined(ESP_PLATFORM)
  const std::uint64_t now = NowMicros();
  if (now >= targetMicros || !clockWorkerRunning_.load()) return;
  ulTaskNotifyTake(pdTRUE, 0);
  if (esp_timer_start_once(static_cast<esp_timer_handle_t>(clockTimer_), targetMicros - now) == ESP_OK)
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
#else
  std::this_thread::sleep_until(std::chrono::steady_clock::time_point(std::chrono::microseconds(targetMicros)));
#endif
}

std::uint64_t SynthSequencer::PulsePeriodMicrosLocked() const
{
  return 60000000ULL / (static_cast<std::uint64_t>(std::max<std::uint16_t>(1, bpm_)) * kSequencerPulsesPerQuarter);
}

void SynthSequencer::AddPendingMidi(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                                    std::size_t& count,
                                    std::size_t trackIndex,
                                    const midi::MidiPacket& packet) const
{
  if (count >= events.size())
  {
    return;
  }
  events[count++] = PendingMidiEvent{trackIndex, packet};
}

void SynthSequencer::EmitPendingMidi(const std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t count) const
{
  MidiSink sink = nullptr;
  void* context = nullptr;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    sink = midiSink_;
    context = midiSinkContext_;
  }
  if (sink == nullptr)
  {
    return;
  }

  for (std::size_t index = 0; index < count; ++index)
  {
    sink(context, events[index].trackIndex, events[index].packet);
  }
}

bool SynthSequencer::ScheduleNoteOffLocked(std::size_t trackIndex, std::uint8_t note, std::uint64_t offPulse)
{
  for (PlaybackNote& activeNote : playbackNotes_)
  {
    if (activeNote.active && activeNote.trackIndex == trackIndex && activeNote.note == note)
    {
      activeNote.offPulse = offPulse;
      return true;
    }
  }

  for (PlaybackNote& activeNote : playbackNotes_)
  {
    if (!activeNote.active)
    {
      activeNote.active = true;
      activeNote.trackIndex = static_cast<std::uint8_t>(trackIndex);
      activeNote.note = note;
      activeNote.offPulse = offPulse;
      return true;
    }
  }
  // A full release schedule must reject the note-on rather than create an untracked voice.
  return false;
}

void SynthSequencer::CollectDueNoteOffsLocked(std::uint64_t pulse,
                                              std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                                              std::size_t& count)
{
  for (PlaybackNote& activeNote : playbackNotes_)
  {
    if (!activeNote.active || activeNote.offPulse > pulse)
    {
      continue;
    }
    AddPendingMidi(events, count, activeNote.trackIndex, midi::MidiPacket::NoteOff(0, activeNote.note, 0));
    activeNote = PlaybackNote{};
  }
}

void SynthSequencer::CollectAllNoteOffsLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count)
{
  StopAuditionLocked(events, count);
  for (PlaybackNote& activeNote : playbackNotes_)
  {
    if (!activeNote.active)
    {
      continue;
    }
    AddPendingMidi(events, count, activeNote.trackIndex, midi::MidiPacket::NoteOff(0, activeNote.note, 0));
    activeNote = PlaybackNote{};
  }

  for (std::size_t trackIndex = 0; trackIndex < tracks_.size(); ++trackIndex)
  {
    AddPendingMidi(events, count, trackIndex, midi::MidiPacket::ControlChange(0, 120, 0));
  }
}

void SynthSequencer::StopAuditionLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count)
{
  if (!auditionActive_)
  {
    return;
  }

  AddPendingMidi(events, count, auditionTrack_, midi::MidiPacket::NoteOff(0, auditionNote_, 0));
  auditionActive_ = false;
}

void SynthSequencer::AuditionScopedNoteLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count)
{
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  const StepData* step = PrimaryHeldStepLocked();
  if (step == nullptr)
  {
    return;
  }

  const NoteEvent* note = ScopedNoteLocked(*step);
  if (note == nullptr)
  {
    return;
  }

  auditionActive_ = true;
  auditionTrack_ = activeTrack_;
  auditionNote_ = note->note;
  AddPendingMidi(events, count, activeTrack_, midi::MidiPacket::NoteOn(0, note->note, note->velocity));
}

void SynthSequencer::ToggleNoteInStepLocked(StepData& step, std::uint8_t note, TrackData& track, std::size_t stepIndex)
{
  if (note > 127) return;
  for (NoteEvent& event : step.notes)
  {
    if (event.active && event.note == note)
    {
      event = NoteEvent{};
      return;
    }
  }

  for (NoteEvent& event : step.notes)
  {
    if (!event.active)
    {
      event.active = true;
      event.note = note;
      event.velocity = track.defaultVelocity;
      event.microstep = 0;
      event.lengthPulses = ClampLengthForStepMicrostep(stepIndex, event.microstep, track.defaultLengthPulses);
      return;
    }
  }
}

void SynthSequencer::ApplyLengthToHeldStepsLocked(std::uint16_t lengthPulses)
{
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  TrackData& track = tracks_[activeTrack_];
  PatternData& pattern = ActivePatternLocked(track);
  track.defaultLengthPulses = lengthPulses;
  for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
  {
    if (!heldSteps_[stepIndex])
    {
      continue;
    }

    StepData& step = pattern.steps[stepIndex];
    if (noteScopeIndex_ < 0)
    {
      for (NoteEvent& note : step.notes)
      {
        if (note.active)
        {
          note.lengthPulses = ClampLengthForStepMicrostep(stepIndex, note.microstep, lengthPulses);
        }
      }
      continue;
    }

    const std::size_t slot = ScopedNoteSlot(step);
    if (slot < step.notes.size())
    {
      step.notes[slot].lengthPulses = ClampLengthForStepMicrostep(stepIndex, step.notes[slot].microstep, lengthPulses);
    }
  }
}

void SynthSequencer::ApplyVelocityToHeldStepsLocked(std::uint8_t velocity)
{
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  TrackData& track = tracks_[activeTrack_];
  PatternData& pattern = ActivePatternLocked(track);
  track.defaultVelocity = velocity;
  for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
  {
    if (!heldSteps_[stepIndex])
    {
      continue;
    }

    StepData& step = pattern.steps[stepIndex];
    if (noteScopeIndex_ < 0)
    {
      for (NoteEvent& note : step.notes)
      {
        if (note.active)
        {
          note.velocity = velocity;
        }
      }
      continue;
    }

    const std::size_t slot = ScopedNoteSlot(step);
    if (slot < step.notes.size())
    {
      step.notes[slot].velocity = velocity;
    }
  }
}

void SynthSequencer::ApplyMicrostepToHeldStepsLocked(std::uint8_t microstep)
{
  if (activeTrack_ >= tracks_.size())
  {
    return;
  }

  microstep = std::min<std::uint8_t>(microstep, kSequencerMicrostepCount - 1);
  TrackData& track = tracks_[activeTrack_];
  PatternData& pattern = ActivePatternLocked(track);
  for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
  {
    if (!heldSteps_[stepIndex])
    {
      continue;
    }

    StepData& step = pattern.steps[stepIndex];
    if (noteScopeIndex_ < 0)
    {
      for (NoteEvent& note : step.notes)
      {
        if (note.active)
        {
          note.microstep = microstep;
          note.lengthPulses = ClampLengthForStepMicrostep(stepIndex, note.microstep, note.lengthPulses);
        }
      }
      continue;
    }

    const std::size_t slot = ScopedNoteSlot(step);
    if (slot < step.notes.size())
    {
      step.notes[slot].microstep = microstep;
      step.notes[slot].lengthPulses = ClampLengthForStepMicrostep(stepIndex, step.notes[slot].microstep, step.notes[slot].lengthPulses);
    }
  }
}

void SynthSequencer::ClearStepLocked(StepData& step)
{
  step = StepData{};
}

void SynthSequencer::QuantizeStepLocked(StepData& step, std::size_t stepIndex)
{
  for (NoteEvent& note : step.notes)
  {
    if (!note.active)
    {
      continue;
    }

    note.microstep = 0;
    note.lengthPulses = ClampLengthForStepMicrostep(stepIndex, note.microstep, note.lengthPulses);
  }
}

void SynthSequencer::CycleNoteScopeLocked()
{
  const StepData* primaryStep = nullptr;
  if (activeTrack_ < tracks_.size())
  {
    const PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
    for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
    {
      if (heldSteps_[stepIndex])
      {
        primaryStep = &pattern.steps[stepIndex];
        break;
      }
    }
  }

  const std::size_t noteCount = primaryStep != nullptr ? ActiveNoteCount(*primaryStep) : 0;
  if (noteCount == 0)
  {
    noteScopeIndex_ = -1;
    return;
  }

  if (noteScopeIndex_ < 0)
  {
    noteScopeIndex_ = 0;
    return;
  }

  ++noteScopeIndex_;
  if (noteScopeIndex_ >= static_cast<int>(noteCount))
  {
    noteScopeIndex_ = -1;
  }
}

void SynthSequencer::NormalizeScopeLocked()
{
  const StepData* primaryStep = nullptr;
  if (activeTrack_ < tracks_.size())
  {
    const PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
    for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
    {
      if (heldSteps_[stepIndex])
      {
        primaryStep = &pattern.steps[stepIndex];
        break;
      }
    }
  }

  const std::size_t noteCount = primaryStep != nullptr ? ActiveNoteCount(*primaryStep) : 0;
  if (noteScopeIndex_ >= static_cast<int>(noteCount))
  {
    noteScopeIndex_ = -1;
  }
}

SynthSequencer::PatternData& SynthSequencer::ActivePatternLocked(TrackData& track)
{
  track.patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  if (track.activePattern >= track.patternCount)
  {
    track.activePattern = track.patternCount - 1;
  }
  return track.patterns[track.activePattern];
}

const SynthSequencer::PatternData& SynthSequencer::ActivePatternLocked(const TrackData& track) const
{
  const std::size_t patternCount = std::clamp<std::size_t>(track.patternCount, 1, kSequencerMaxPatternCount);
  const std::size_t activePattern = std::min(track.activePattern, patternCount - 1);
  return track.patterns[activePattern];
}

const SynthSequencer::StepData* SynthSequencer::PrimaryHeldStepLocked() const
{
  if (activeTrack_ >= tracks_.size())
  {
    return nullptr;
  }

  const PatternData& pattern = ActivePatternLocked(tracks_[activeTrack_]);
  for (std::size_t stepIndex = 0; stepIndex < heldSteps_.size(); ++stepIndex)
  {
    if (heldSteps_[stepIndex])
    {
      return &pattern.steps[stepIndex];
    }
  }
  return nullptr;
}

std::size_t SynthSequencer::ActiveNoteCount(const StepData& step) const
{
  std::size_t count = 0;
  for (const NoteEvent& note : step.notes)
  {
    if (note.active)
    {
      ++count;
    }
  }
  return count;
}

std::size_t SynthSequencer::ScopedNoteSlot(const StepData& step) const
{
  if (noteScopeIndex_ < 0)
  {
    return step.notes.size();
  }

  std::array<std::size_t, 8> slots{};
  std::size_t slotCount = 0;
  for (std::size_t slot = 0; slot < step.notes.size(); ++slot)
  {
    if (step.notes[slot].active)
    {
      slots[slotCount++] = slot;
    }
  }
  // GCC's small-range std::sort expansion probes its 16-item insertion-sort
  // threshold and triggers -Warray-bounds for this fixed 8-item buffer on
  // Xtensa. An explicit insertion sort keeps every access visibly bounded.
  for (std::size_t index = 1; index < slotCount; ++index)
  {
    const std::size_t slot = slots[index];
    std::size_t insertionIndex = index;
    while (insertionIndex > 0 && step.notes[slot].note < step.notes[slots[insertionIndex - 1]].note)
    {
      slots[insertionIndex] = slots[insertionIndex - 1];
      --insertionIndex;
    }
    slots[insertionIndex] = slot;
  }

  return noteScopeIndex_ >= 0 && static_cast<std::size_t>(noteScopeIndex_) < slotCount
             ? slots[static_cast<std::size_t>(noteScopeIndex_)]
             : step.notes.size();
}

const SynthSequencer::NoteEvent* SynthSequencer::ScopedNoteLocked(const StepData& step) const
{
  const std::size_t slot = ScopedNoteSlot(step);
  return slot < step.notes.size() && step.notes[slot].active ? &step.notes[slot] : nullptr;
}

void SynthSequencer::PopulateEditValueStateLocked(SequencerUiState& state, const StepData* step) const
{
  const NoteEvent* note = step != nullptr ? ScopedNoteLocked(*step) : nullptr;
  if (note == nullptr && step != nullptr)
  {
    for (const NoteEvent& event : step->notes)
    {
      if (event.active)
      {
        note = &event;
        break;
      }
    }
  }

  switch (editLane_)
  {
    case SequencerEditLane::Length:
      state.editValueCount = kLengthBuckets.size();
      state.editValueIndex = 0;
      if (note != nullptr)
      {
        for (std::size_t index = 0; index < kLengthBuckets.size(); ++index)
        {
          if (note->lengthPulses <= kLengthBuckets[index])
          {
            state.editValueIndex = index;
            break;
          }
          state.editValueIndex = index;
        }
      }
      break;
    case SequencerEditLane::Velocity:
      state.editValueCount = kVelocityBuckets.size();
      state.editValueIndex = 0;
      if (note != nullptr)
      {
        std::uint8_t bestDistance = 0xFF;
        for (std::size_t index = 0; index < kVelocityBuckets.size(); ++index)
        {
          const std::uint8_t distance = static_cast<std::uint8_t>(std::abs(static_cast<int>(note->velocity) - static_cast<int>(kVelocityBuckets[index])));
          if (distance < bestDistance)
          {
            bestDistance = distance;
            state.editValueIndex = index;
          }
        }
      }
      break;
    case SequencerEditLane::Micro:
      state.editValueCount = kSequencerMicrostepCount;
      state.editValueIndex = note != nullptr ? std::min<std::size_t>(note->microstep, kSequencerMicrostepCount - 1) : 0;
      break;
    case SequencerEditLane::Note:
    default:
      state.editValueCount = 0;
      state.editValueIndex = 0;
      break;
  }
}

std::uint16_t SynthSequencer::ClampLengthForStepMicrostep(std::size_t stepIndex, std::uint8_t microstep, std::uint16_t lengthPulses) const
{
  const std::uint16_t startPulse = static_cast<std::uint16_t>(stepIndex * kSequencerPulsesPerStep +
                                                             microstep * kSequencerPulsesPerMicrostep);
  const std::uint16_t patternPulses = static_cast<std::uint16_t>(kSequencerStepCount * kSequencerPulsesPerStep);
  const std::uint16_t remaining = startPulse < patternPulses ? static_cast<std::uint16_t>(patternPulses - startPulse) : 1;
  return std::max<std::uint16_t>(1, std::min(lengthPulses, remaining));
}

std::string SynthSequencer::ScopeLabelLocked(const StepData* step) const
{
  if (noteScopeIndex_ < 0 || step == nullptr)
  {
    return "ALL";
  }

  const std::size_t noteCount = ActiveNoteCount(*step);
  if (noteCount == 0 || static_cast<std::size_t>(noteScopeIndex_) >= noteCount)
  {
    return "ALL";
  }
  return std::to_string(noteScopeIndex_ + 1) + "/" + std::to_string(noteCount);
}

std::string SynthSequencer::ValueLabelLocked(const StepData* step) const
{
  if (step == nullptr || ActiveNoteCount(*step) == 0)
  {
    return SequencerEditLaneLabel(editLane_);
  }

  const NoteEvent* note = nullptr;
  if (noteScopeIndex_ >= 0)
  {
    const std::size_t slot = ScopedNoteSlot(*step);
    if (slot < step->notes.size())
    {
      note = &step->notes[slot];
    }
  }
  if (note == nullptr)
  {
    for (const NoteEvent& event : step->notes)
    {
      if (event.active)
      {
        note = &event;
        break;
      }
    }
  }
  if (note == nullptr)
  {
    return SequencerEditLaneLabel(editLane_);
  }

  switch (editLane_)
  {
    case SequencerEditLane::Note:
      return std::to_string(ActiveNoteCount(*step)) + " notes";
    case SequencerEditLane::Length:
      return "LEN " + std::to_string((note->lengthPulses + kSequencerPulsesPerStep - 1) / kSequencerPulsesPerStep);
    case SequencerEditLane::Velocity:
      return "VEL " + std::to_string(note->velocity);
    case SequencerEditLane::Micro:
      return "STEP " + std::to_string(note->microstep + 1);
    default:
      return "SEQ";
  }
}

}  // namespace synth
