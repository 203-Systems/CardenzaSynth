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
#if defined(__EMSCRIPTEN__)
  // The web build has no threads, so no clock worker runs; the host pumps the clock instead.
  // Processes every pulse due by nowMicros on the host's clock: ClockLoop's steps (transport and
  // arpeggiator) with its sleeps replaced by returns. The web host passes audio time (frames
  // rendered so far) before each audio block, so pulses land on exact block positions whatever the
  // main thread does.
  void PumpClock(std::uint64_t nowMicros);
  void PumpClock() { PumpClock(NowMicros()); }
#endif
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
  // Bit n set = step n of that track's pattern holds notes (0 for an empty or missing pattern).
  std::uint16_t PatternStepMask(std::size_t trackIndex, std::size_t patternIndex) const;

  void PressStep(std::size_t stepIndex, const std::uint8_t* heldNotes, std::size_t heldNoteCount);
  void ReleaseStep(std::size_t stepIndex);
  void ClearHeldSteps();
  void ClearStep(std::size_t stepIndex);
  void ClearHeldStepEvents();
  bool CopyStep(std::size_t sourceStep, std::size_t destinationStep);
  bool StepHasEvents(std::size_t stepIndex) const;
  // Pitches of the active notes of `step` in the active track's active pattern, ascending.
  // Returns how many were written; 0 for an empty or out-of-range step.
  std::size_t StepNotes(std::size_t step, std::array<std::uint8_t, 8>& notes) const;
  // Every note of one step of a track's active pattern, with its timing (the piano roll).
  struct StepNoteView {
    std::uint8_t note = 0;
    std::uint8_t velocity = 0;
    std::uint16_t lengthPulses = 0;
    std::uint8_t microstep = 0;
  };
  std::size_t StepNoteEvents(std::size_t track, std::size_t step, std::array<StepNoteView, 8>& notes) const;
  // Step recording: the step's notes become exactly these (one step long, on the grid).
  bool SetStepNotes(std::size_t track, std::size_t step, const std::uint8_t* notes, std::size_t count, std::uint8_t velocity);
  // Adds one timed note to a pattern of a track (a note of the same pitch on that step is
  // replaced). False when the step already holds eight other notes or the pattern does not exist.
  bool InsertNote(std::size_t track, std::size_t pattern, std::size_t step, std::uint8_t microstep, std::uint8_t note,
                  std::uint8_t velocity, std::uint16_t lengthPulses);
  // Pulses processed since playback started (0 while stopped): where a played note sits in time.
  std::uint64_t PlayPulse() const;
  std::uint8_t SelectedNoteVelocity(std::uint8_t pitch) const;
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

  // Live overdub. Notes are written only while playing and armed, into the track's active
  // pattern at its playback position, quantized to the nearest microstep. The sequencer never
  // sounds the live note itself: the caller already played it.
  void SetRecordArmed(bool armed);
  bool RecordArmed() const;
  void RecordNoteOn(std::size_t track, std::uint8_t note, std::uint8_t velocity);
  void RecordNoteOff(std::size_t track, std::uint8_t note);

  // Live arpeggiators, one per track, each with its own settings and held notes; they run side by
  // side. The caller hands a track's arp every note a key press makes (instead of sounding them)
  // and the arp plays the held set through the MIDI sink, one note per rate period, like sequenced
  // notes. The first press while the arp is idle sounds at once; later notes follow the transport
  // grid while playing (swung like sequenced sixteenths at 1/16), else a free clock at the current
  // tempo. Stop() cuts the sounding notes but lets held or latched arps run on free.
  // While playing and armed, every arp note is overdubbed into its track's active pattern.
  // Settings are clamped; mode Off, or latch switched off with no key held, releases the arp.
  void SetArpSettings(std::size_t track, const ArpSettings& settings);
  ArpSettings GetArpSettings(std::size_t track) const;
  // Ignored while the track's mode is Off. With latch on, the first press after every key was
  // released replaces the latched set. Velocity 0 is a note-off.
  void ArpNoteOn(std::size_t track, std::uint8_t note, std::uint8_t velocity);
  void ArpNoteOff(std::size_t track, std::uint8_t note);
  // Drops held and latched notes and silences the sounding note of every arp (RestoreState() and
  // Reset() do this too), or of one track's arp.
  void ArpReleaseAll();
  void ArpRelease(std::size_t track);
  // Mode on and notes held or latched: on any track, or on one.
  bool ArpActive() const;
  bool ArpActive(std::size_t track) const;

#if defined(CARDENZA_SEQUENCER_TEST_HOOKS)
  // Test only. Call first: the clock worker is never started, and AdvancePulsesForTest() runs
  // the clock by hand, one pulse per step, exactly when the worker would not idle.
  void UseManualClockForTest();
  void AdvancePulsesForTest(std::size_t pulses);
#endif

  // Switches immediately while stopped; while playing, at the track's next loop boundary.
  // patternIndex == PatternCount(track) creates a new pattern slot. Queueing the active
  // pattern cancels a pending switch.
  bool QueuePattern(std::size_t track, std::size_t patternIndex);

  // 50 is straight, 75 the hardest swing: odd sixteenths are delayed.
  void SetSwing(std::uint8_t percent);
  std::uint8_t Swing() const;

  // Each track loops at its own length (1..16 steps); later steps keep their data.
  void SetTrackStepCount(std::size_t track, std::size_t steps);
  std::size_t TrackStepCount(std::size_t track) const;

  // Selection editing without audition. The selection is the held-step set of the active track.
  void SetStepSelected(std::size_t step, bool selected);
  void ClearSelectedSteps();
  bool ToggleNoteOnSelectedSteps(std::uint8_t note);
  void SetEditLane(SequencerEditLane lane);
  void CycleEditLane();
  void CycleNoteScope();
  // Relative edit of the current lane for the scoped note (or every note) of each selected
  // step. Returns true when any value changed.
  bool AdjustSelectedSteps(int delta, bool coarse);

  // Song (arrangement) mode. With the switch on and at least one row, Play() starts at the cued
  // row. Each row applies at a bar line: every track switches to the row's pattern (-1 leaves the
  // track silent for the row without touching mute/enable), restarts at step 0, and drops a
  // queued pattern switch. A row lasts repeats bars of the global clock, whatever the tracks'
  // step counts. After the last row the song loops to row 0, or playback stops when looping is
  // off. Flipping the switch while playing takes effect at the next bar line; switching off keeps
  // the patterns of the row that was playing and lets every track loop on from where it is.
  // Edits to the playing row are heard the next time a row starts; its length is read at each
  // bar line. The switch is a performance control: projects do not store it.
  void SetSongMode(bool enabled);
  bool SongMode() const;
  void SetSongLoop(bool loop);
  bool SongLoop() const;
  std::size_t SongLength() const;
  // A default row (every track on pattern 0, one bar) when out of range.
  SongRow SongRowAt(std::size_t row) const;
  // pattern is -1..PatternCount(track) - 1; row < SongLength().
  bool SetSongPattern(std::size_t row, std::size_t track, int pattern);
  // 1..kSongMaxRepeats bars.
  bool SetSongRepeats(std::size_t row, std::uint8_t repeats);
  // at <= SongLength() < kSongMaxRows; every pattern index and the repeats must be valid.
  bool InsertSongRow(std::size_t at, const SongRow& row);
  // Deleting the playing row ends it at the next bar line; the row that takes its place follows.
  bool DeleteSongRow(std::size_t row);
  // Playing with song mode on: jump to the row at the next bar line (also where a song switched
  // on mid-play takes over). Otherwise: the start row for Play() and for switching song mode on
  // while playing; it stays until cued again and is clamped to the song. row < SongLength().
  void CueSongRow(std::size_t row);

  void SetTempo(std::uint16_t bpm);
  std::uint16_t Tempo() const;
  void SetTrackEnabled(std::size_t track, bool enabled);
  bool TrackEnabled(std::size_t track) const;
  bool ClearPatternEvents(std::size_t track, std::size_t patternIndex);
  std::size_t PatternCount(std::size_t track) const;
  std::size_t ActivePattern(std::size_t track) const;

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
    std::size_t stepCount = kSequencerStepCount;
  };

  struct ProjectState {
    std::uint16_t bpm = 120;
    std::array<TrackData, kSynthTrackCount> tracks{};
    std::uint8_t swing = 50;
    // Rows past songLength are ignored and come back as default rows.
    std::array<SongRow, kSongMaxRows> song{};
    std::size_t songLength = 0;
    bool songLoop = true;
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

  // A live note whose length is still open until its note-off or the track's loop wrap.
  struct HeldRecording {
    std::uint64_t onPulse = 0;
    std::uint64_t startPulse = 0;
    std::size_t pattern = 0;
    std::uint8_t trackIndex = 0;
    std::uint8_t step = 0;
    std::uint8_t note = 0;
    bool active = false;
  };

  // A recorded note quantized ahead of the playhead must not sound again in the same pass.
  struct SuppressedNote {
    std::uint64_t pulse = 0;
    std::uint8_t trackIndex = 0;
    std::uint8_t note = 0;
    bool active = false;
  };

  static constexpr std::size_t kMaxPlaybackNotes = 96;
  static constexpr std::size_t kMaxPendingMidiEvents = 128;
  static constexpr std::size_t kMaxHeldRecordings = 16;
  static constexpr std::size_t kMaxSuppressedNotes = 16;
  static constexpr std::size_t kNoPattern = kSequencerMaxPatternCount;

  void EnsureClockWorkerStarted();
  void StopClockWorker();
#if defined(ESP_PLATFORM)
  static void ClockTaskEntry(void* context);
#endif
  void ClockLoop();
  // Whether the clock must run pulses (transport playing or arp in use). Also hands over, once,
  // when an arp started from idle wants its first pulse (0 = none).
  bool ClockNeeded(std::uint64_t& startMicros);
  void ProcessPulse();
  void ProcessTransportPulseLocked(std::uint64_t pulse,
                                   std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                                   std::size_t& count);
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

  static std::size_t TrackStepCountOf(const TrackData& track);
  std::uint16_t SwingDelayPulsesLocked() const;
  std::uint16_t NoteOffsetPulsesLocked(std::size_t stepIndex, std::uint8_t microstep) const;
  void FireStepNotesLocked(std::size_t trackIndex,
                           const StepData& step,
                           std::size_t stepIndex,
                           std::uint32_t targetOffset,
                           std::uint64_t pulse,
                           std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                           std::size_t& count);
  bool ConsumeSuppressedNoteLocked(std::size_t trackIndex, std::uint8_t note, std::uint64_t pulse);
  void HandleLoopBoundaryLocked(std::size_t trackIndex, std::uint64_t pulse);
  void StopLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  // Runs on every bar line of the global clock; false when the song ended and playback stopped.
  bool HandleSongBarLocked(std::uint64_t pulse, std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void StartSongRowLocked(std::size_t row, std::uint64_t pulse);
  void LeaveSongLocked();
  // Song references to a removed pattern become -1; later indices shift down when `shift`.
  void RemoveSongPatternLocked(std::size_t trackIndex, std::size_t patternIndex, bool shift);
  void ResetSongLocked();
  void ResetPlaybackPositionsLocked();
  void ApplyQueuedPatternsLocked();
  void RecordNoteOnLocked(std::size_t trackIndex, std::uint8_t note, std::uint8_t velocity);
  void RecordNoteOffLocked(std::size_t trackIndex, std::uint8_t note);
  void FinalizeHeldRecordingLocked(HeldRecording& recording, std::uint64_t pulse);
  void FinalizeAllHeldRecordingsLocked();
  std::uint64_t RecordPulseLocked(std::size_t trackIndex) const;
  void ReleaseTrackNotesLocked(std::size_t trackIndex,
                               std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                               std::size_t& count);
  void ClearSelectionLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);

  // Arpeggiators (one ArpVoice per track; ArpTrack() is its track).
  struct ArpVoice;
  std::size_t ArpTrack(const ArpVoice& arp) const;
  bool ArpActiveLocked(const ArpVoice& arp) const;
  bool AnyArpActiveLocked() const;
  // Active, or its last note still sounding: the clock must run.
  bool ArpClockLocked(const ArpVoice& arp) const;
  bool AnyArpClockLocked() const;
  std::uint16_t ArpPeriodLocked(const ArpVoice& arp) const;
  std::uint16_t ArpSwingLocked(std::uint64_t gridPulse, std::uint16_t period) const;
  // Candidate `index` of the pattern: held note index % count, raised by index / count octaves.
  bool ArpCandidateLocked(const ArpVoice& arp, std::size_t index, std::uint8_t& note, std::uint8_t& velocity) const;
  bool NextArpNoteLocked(ArpVoice& arp, std::uint8_t& note, std::uint8_t& velocity);
  void ResetArpOrderLocked(ArpVoice& arp);
  void RemoveArpKeyLocked(ArpVoice& arp, std::size_t index);
  // Plays the next arp note at arp.pulse; a still sounding arp note ends first.
  void FireArpNoteLocked(ArpVoice& arp, std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void CutArpNoteLocked(ArpVoice& arp, std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void ReleaseArpLocked(ArpVoice& arp, std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  void ReleaseAllArpsLocked(std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events, std::size_t& count);
  // Schedules the next arp note on the first transport grid line at or after transportPulse
  // (which arp.pulse stands for) that lies at least half a period after the last arp note.
  void AlignArpToTransportLocked(ArpVoice& arp, std::uint64_t transportPulse);
  void AlignArpsToTransportLocked(std::uint64_t transportPulse);
  void RescheduleArpLocked(ArpVoice& arp);
  void ProcessArpPulseLocked(ArpVoice& arp, std::uint64_t transportPulse,
                             std::array<PendingMidiEvent, kMaxPendingMidiEvents>& events,
                             std::size_t& count);

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
  std::uint8_t swing_ = 50;
  bool recordArmed_ = false;
  // Per-track loop position in pulses: the next pulse to process, and the last processed one.
  std::array<std::uint16_t, kSynthTrackCount> trackNextPulse_{};
  std::array<std::uint16_t, kSynthTrackCount> trackCurrentPulse_{};
  std::array<bool, kSynthTrackCount> trackStarted_{};
  std::array<std::size_t, kSynthTrackCount> queuedPattern_{};
  // Pattern of the previous pass: swung notes of its last step spill past the loop wrap.
  std::array<std::size_t, kSynthTrackCount> previousPassPattern_{};
  // Pulses the previous pass ran: the loop length, or less when a song row cut it short.
  std::array<std::uint16_t, kSynthTrackCount> previousPassPulses_{};
  // Song arrangement and its playback position.
  std::array<SongRow, kSongMaxRows> song_{};
  std::size_t songLength_ = 0;
  bool songLoop_ = true;
  bool songMode_ = false;
  // True while playback follows the song; changes only on bar lines.
  bool songEngaged_ = false;
  std::size_t songRow_ = 0;
  std::size_t songBar_ = 0;
  std::size_t songStartRow_ = 0;
  bool songJumpPending_ = false;
  // May equal songLength_: the jump then lands past the end (loop to row 0, or stop).
  std::size_t songJumpRow_ = 0;
  std::array<bool, kSynthTrackCount> songSilent_{};
  std::array<HeldRecording, kMaxHeldRecordings> heldRecordings_{};
  std::array<SuppressedNote, kMaxSuppressedNotes> suppressedNotes_{};
  bool auditionActive_ = false;
  std::size_t auditionTrack_ = 0;
  std::uint8_t auditionNote_ = 0;
  // Arpeggiator state, one voice per track. Keys are the held (or latched) notes in press order.
  struct ArpKey {
    std::uint8_t note = 0;
    std::uint8_t velocity = 0;
    bool down = false;  // still held; a latched note is kept with down == false
  };
  struct ArpVoice {
    ArpSettings settings{};
    std::array<ArpKey, kArpMaxNotes> keys{};
    std::size_t keyCount = 0;
    // Arp clock: counts the pulses processed while the arp is in use; during playback it advances
    // with absolutePulse_. pulse is the next pulse to process.
    std::uint64_t pulse = 0;
    std::uint64_t nextPulse = 0;
    std::uint64_t lastOnPulse = 0;
    bool hasLastOn = false;
    int soundingNote = -1;
    std::uint64_t offPulse = 0;
    // Pattern position: the last pitch (UP, DOWN, UP-DN, RANDOM) and the next index (ORDER).
    int lastNote = -1;
    bool descending = false;
    std::size_t orderIndex = 0;
    std::uint32_t random = 0x9E3779B9u;
    std::uint32_t tick = 0;
    int lastPlayed = -1;
  };
  std::array<ArpVoice, kSynthTrackCount> arps_{};
  // When an arp started from idle, the time its first pulse belongs to (0 = none).
  std::uint64_t clockStartMicros_ = 0;
  MidiSink midiSink_ = nullptr;
  void* midiSinkContext_ = nullptr;
  std::atomic<bool> clockWorkerRunning_{false};
  std::atomic<bool> clockWorkerExited_{true};

#if defined(ESP_PLATFORM)
  void* clockTimer_ = nullptr;
#else
  std::thread clockThread_;
#endif
#if defined(__EMSCRIPTEN__)
  // ClockLoop's locals, kept between PumpClock() calls.
  std::uint64_t pumpNextPulseMicros_ = 0;
  bool pumpIdle_ = true;
#endif
};

}  // namespace synth
