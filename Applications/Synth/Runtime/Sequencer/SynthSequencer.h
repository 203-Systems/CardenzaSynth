#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string>

#if !defined(ESP_PLATFORM)
#include <thread>
#endif

#include "Applications/Synth/Runtime/Sequencer/SynthSequencerTypes.h"
#include "OS/Framework/Midi/MidiPacket.h"

namespace synth {

class SynthSequencer {
public:
  using MidiSink = void (*)(void* context, std::size_t trackIndex, const midi::MidiPacket& packet);

  SynthSequencer();
  ~SynthSequencer();

  void SetMidiSink(MidiSink sink, void* context);
  void SetActiveTrack(std::size_t trackIndex);

  void Play();
  void Resume();
  void Stop();
  void TogglePlayback();
  bool Playing() const;
  void AdjustTempo(int delta);
  void ToggleTrackMuted();
  void ToggleTrackEnabled();
  void Reset();
  void StepPattern(int delta);
  void AdjustPatternCount(int delta);
  bool SelectOrCreatePattern(std::size_t patternIndex);
  bool DeletePattern(std::size_t patternIndex);
  bool CopyPattern(std::size_t sourcePattern, std::size_t destinationPattern);
  bool PatternHasEvents(std::size_t patternIndex) const;

  void PressStep(std::size_t stepIndex, const std::uint8_t* heldNotes, std::size_t heldNoteCount);
  void ReleaseStep(std::size_t stepIndex);
  void ClearHeldSteps();
  void ClearStep(std::size_t stepIndex);
  void ClearHeldStepEvents();
  bool CopyStep(std::size_t sourceStep, std::size_t destinationStep);
  bool StepHasEvents(std::size_t stepIndex) const;
  void QuantizeStep(std::size_t stepIndex);
  void QuantizeHeldSteps();
  void QuantizeActivePattern();
  void QuantizePattern(std::size_t patternIndex);
  bool IsStepHeld(std::size_t stepIndex) const;
  bool HasHeldSteps() const;

  bool PressEditLane(std::size_t row);
  bool ReleaseEditLane(std::size_t row);
  bool HandlePerformanceKeyPress(std::size_t row, std::size_t column, std::uint8_t note);

  SequencerUiState BuildUiState(std::size_t activeTrack) const;

  // Project snapshots are heap-owned: the full four-track data exceeds the firmware task stack.
  struct NoteEvent {
    std::uint8_t note = 60;
    std::uint8_t velocity = 110;
    std::uint16_t lengthPulses = kSequencerPulsesPerStep;
    std::uint8_t microstep = 0;
    bool active = false;
  };

  struct StepData {
    std::array<NoteEvent, 8> notes{};
  };

  struct PatternData {
    std::array<StepData, kSequencerStepCount> steps{};
  };

  // Patterns are allocated on first write: inline 16-pattern banks for every track cost about 48 KB
  // of internal RAM, which the firmware cannot spare. Unallocated patterns read as empty.
  class PatternBank {
  public:
    PatternBank() = default;
    PatternBank(const PatternBank& other) { (void)CopyFrom(other); }
    PatternBank& operator=(const PatternBank& other);
    PatternBank(PatternBank&&) noexcept = default;
    PatternBank& operator=(PatternBank&&) noexcept = default;

    // Allocates on demand; if memory runs out, writes land in a discarded scratch pattern.
    PatternData& operator[](std::size_t index);
    const PatternData& operator[](std::size_t index) const;
    static constexpr std::size_t size() { return kSequencerMaxPatternCount; }

    bool Allocated(std::size_t index) const;
    bool Ensure(std::size_t index);
    // Allocates every pattern `other` holds, so a following copy cannot fail halfway.
    bool Reserve(const PatternBank& other);
    bool CopyFrom(const PatternBank& other);
    void Release(std::size_t index);
    void Swap(std::size_t left, std::size_t right);
    void Clear();

  private:
    std::array<std::unique_ptr<PatternData>, kSequencerMaxPatternCount> patterns_{};
  };

  struct TrackData {
    PatternBank patterns{};
    std::size_t patternCount = 1;
    std::size_t activePattern = 0;
    std::uint8_t defaultVelocity = 110;
    std::uint16_t defaultLengthPulses = kSequencerPulsesPerStep;
    bool enabled = true;
    bool muted = false;
  };

  struct ProjectState {
    std::uint16_t bpm = 120;
    std::array<TrackData, kSynthTrackCount> tracks{};
  };
  std::unique_ptr<ProjectState> CaptureState() const;
  static bool ValidateState(const ProjectState& state);
  bool RestoreState(const ProjectState& state);

private:

  struct PlaybackNote {
    std::uint64_t offPulse = 0;
    std::uint8_t trackIndex = 0;
    std::uint8_t note = 0;
    bool active = false;
  };

  struct PendingMidiEvent {
    std::size_t trackIndex = 0;
    midi::MidiPacket packet;
  };

  static constexpr std::size_t kMaxPlaybackNotes = 96;
  static constexpr std::size_t kMaxPendingMidiEvents = 128;

  void EnsureClockWorkerStarted();
  void StopClockWorker();
#if defined(ESP_PLATFORM)
  static void ClockTaskEntry(void* context);
#endif
  void ClockLoop();
  void ProcessPulse();
  std::uint64_t NowMicros() const;
  void SleepUntilMicros(std::uint64_t targetMicros) const;
  std::uint64_t PulsePeriodMicrosLocked() const;
  void AddPendingMidi(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                      std::size_t& count,
                      std::size_t trackIndex,
                      const midi::MidiPacket& packet) const;
  void EmitPendingMidi(const std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t count) const;
  bool ScheduleNoteOffLocked(std::size_t trackIndex, std::uint8_t note, std::uint64_t offPulse);
  void CollectDueNoteOffsLocked(std::uint64_t pulse,
                                std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                                std::size_t& count);
  void CollectAllNoteOffsLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void StopAuditionLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void AuditionScopedNoteLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);

  void ToggleNoteInStepLocked(StepData& step, std::uint8_t note, TrackData& track, std::size_t stepIndex);
  void ApplyLengthToHeldStepsLocked(std::uint16_t lengthPulses);
  void ApplyVelocityToHeldStepsLocked(std::uint8_t velocity);
  void ApplyMicrostepToHeldStepsLocked(std::uint8_t microstep);
  void ClearStepLocked(StepData& step);
  void QuantizeStepLocked(StepData& step, std::size_t stepIndex);
  void CycleNoteScopeLocked();
  void NormalizeScopeLocked();
  void PopulateEditValueStateLocked(SequencerUiState& state, const StepData* step) const;
  PatternData& ActivePatternLocked(TrackData& track);
  const PatternData& ActivePatternLocked(const TrackData& track) const;
  const StepData* PrimaryHeldStepLocked() const;
  std::size_t ActiveNoteCount(const StepData& step) const;
  std::size_t ScopedNoteSlot(const StepData& step) const;
  const NoteEvent* ScopedNoteLocked(const StepData& step) const;
  std::uint16_t ClampLengthForStepMicrostep(std::size_t stepIndex, std::uint8_t microstep, std::uint16_t lengthPulses) const;
  std::string ScopeLabelLocked(const StepData* step) const;
  std::string ValueLabelLocked(const StepData* step) const;

  mutable std::mutex mutex_;
  // Lock before mutex_: MIDI batches and transport changes must keep generation order.
  mutable std::mutex outputMutex_;
  std::array<TrackData, kSynthTrackCount> tracks_{};
  std::array<bool, kSequencerStepCount> heldSteps_{};
  std::array<bool, kKeyboardMatrixRows> heldEditLanes_{};
  std::array<PlaybackNote, kMaxPlaybackNotes> playbackNotes_{};
  std::size_t activeTrack_ = 0;
  SequencerEditLane editLane_ = SequencerEditLane::Note;
  int noteScopeIndex_ = -1;
  bool playing_ = false;
  std::size_t playbackStep_ = 0;
  std::uint64_t absolutePulse_ = 0;
  std::uint16_t bpm_ = 120;
  bool auditionActive_ = false;
  std::size_t auditionTrack_ = 0;
  std::uint8_t auditionNote_ = 0;
  MidiSink midiSink_ = nullptr;
  void* midiSinkContext_ = nullptr;
  std::atomic<bool> clockWorkerRunning_{false};
  std::atomic<bool> clockWorkerExited_{true};

#if defined(ESP_PLATFORM)
  void* clockTimer_ = nullptr;
#else
  std::thread clockThread_;
#endif
};

}  // namespace synth
