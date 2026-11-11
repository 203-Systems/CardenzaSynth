#pragma once

#include "Applications/Synth/Runtime/Sampler/SamplerTrackState.h"
#include "Library/Synth/Core/SynthTypes.h"
#include "Library/Synth/Audio/SynthAudioRuntime.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdio>
#include <memory>

namespace synth {

class SamplerManager {
public:
  // With a card a recording goes straight to a file on it (Session/take-<track>.wav, on the card
  // for good about once a second); without one it fits in what RAM is free. The file stays until the
  // take is saved or discarded, so a power cut loses at most the last second.
  static constexpr std::uint32_t kMaxRecordSeconds = 60;

  ~SamplerManager();
  struct PreparedSample {
    std::string fileName;
    audio::SharedSampleBuffer frames;
    std::uint32_t sampleRate = audio::kSampleRate;
  };

  void OnTrackEngineChanged(std::size_t trackIndex, SynthEngineKind kind);
  void Tick();

  bool BeginRecord(std::size_t trackIndex, bool replaceDraft = false);
  bool StopRecord(std::size_t trackIndex);
  void CancelRecord(std::size_t trackIndex);
  void DiscardDraftTake(std::size_t trackIndex);
  void SetDraftTrimWindow(std::size_t trackIndex, int start127, int end127);
  void SetDraftLoopEnabled(std::size_t trackIndex, bool enabled);
  void EnterDraftNaming(std::size_t trackIndex);
  // The first TAKE_nn that is free.
  std::string FreeTakeName() const;
  bool CommitDraftName(std::size_t trackIndex, std::string_view name);
  bool ConfirmOverwriteDraft(std::size_t trackIndex);
  void CancelNameEntry(std::size_t trackIndex);
  bool PreviewCurrentSample(std::size_t trackIndex, std::uint8_t rootNote);
  bool SelectBrowserSample(std::size_t trackIndex, std::string_view id);
  bool RenameBrowserSample(std::size_t trackIndex, std::string_view id, std::string_view newName,
                           std::string* renamedFile = nullptr);
  // The first request fixes the target; a second request for the same file confirms.
  bool DeleteBrowserSample(std::size_t trackIndex, std::string_view id);
  bool RenameCurrentSample(std::size_t trackIndex, std::string_view newName);
  bool DeleteCurrentSample(std::size_t trackIndex);

  // Brings back the takes a power cut or an app switch left on the card, on the tracks that are
  // samplers. Returns the track of the first one, -1 when there is none.
  int RestoreTakes();
  // The app is going away, not the takes: a recording ends as a take and the files stay.
  void KeepTakesOnCard();
  SamplerTrackUiState BuildUiState(std::size_t trackIndex) const;
  // How loud `columns` equal stretches of [firstFrame, lastFrame) of the track's sample are, 0..255
  // (the chop's waveform at any zoom). Measured for a sample in RAM; for one played from the card, the
  // stretches looked at when it was loaded (a chop's) or nothing. False without a sample.
  bool WaveformPeaks(std::size_t trackIndex, std::size_t firstFrame, std::size_t lastFrame, std::uint8_t* peaks, std::size_t columns) const;
  bool Previewing(std::size_t trackIndex) const;
  // The track the microphone is recording for, -1 when it is off.
  int RecordingTrack() const { return activeRecordingTrack_; }
  bool ConsumeTrimReset(std::size_t trackIndex);
  static bool ValidateSampleReference(std::string_view id);
  static bool PrepareSample(std::string_view id, PreparedSample& sample);
  bool ApplySample(std::size_t trackIndex, const PreparedSample& sample);

private:
  struct TrackState {
    SamplerTrackUiState ui;
    audio::SharedSampleBuffer draftFrames;
    // While recording: the frames so far, in RAM when recordBuffer is set, else in the take file.
    std::shared_ptr<audio::SampleBuffer> recordBuffer;
    std::size_t recordedFrames = 0;
    std::int32_t recordBinPeak = 0;
    std::size_t recordBinFrames = 0;
    std::size_t recordSyncedFrames = 0;
    bool takeOnCard = false;
    audio::SharedSampleBuffer currentFrames;
    std::uint32_t currentSampleRate = audio::kSampleRate;
    std::string pendingDeleteFileName;
    // A chop's sample on the card: a look at it every 1/kOverviewBins, taken when it was loaded (PSRAM).
    std::unique_ptr<std::uint8_t[]> overview;
    bool trimResetPending = false;
    bool isSampler = false;  // the engine plays a sample file: the sampler or the chop
    bool records = false;    // the sampler: its track records
    std::uint8_t previewNote = 60;
    std::chrono::steady_clock::time_point previewTickAt{};
  };

  static constexpr std::size_t kOverviewBins = 256;
  static constexpr std::size_t kOverviewProbeFrames = 256;
  static constexpr std::size_t kPeakReadsPerColumn = 256;
  struct PeakCache {
    const audio::SampleBuffer* buffer = nullptr;
    std::size_t first = 0;
    std::size_t last = 0;
    std::size_t columns = 0;
    std::array<std::uint8_t, 240> peaks{};
  };
  void BuildOverview(TrackState& state);
  bool IsValidSamplerTrack(std::size_t trackIndex) const;
  bool CanReplaceSample(std::size_t trackIndex);
  bool FileUsedByOtherTrack(std::size_t trackIndex, std::string_view fileName) const;
  static audio::SharedSampleBuffer ActiveFrames(const TrackState& state);
  void StopPreview(std::size_t trackIndex, TrackState& state);
  void SyncAudioSample(std::size_t trackIndex, const TrackState& state);
  void PumpInput(TrackState& state);
  void FinishDraft(TrackState& state);
  bool SaveDraftAs(TrackState& state, std::string_view name, bool overwrite);
  // The take file of the running recording, finished into a sample when `keep`.
  void CloseTakeFile(TrackState& state, bool keep);
  void RemoveTakeFile(std::size_t trackIndex);

  std::array<TrackState, kSynthTrackCount> tracks_{};
  mutable std::array<PeakCache, 2> peakCache_{};
  mutable std::size_t peakCacheNext_ = 0;
  int activeRecordingTrack_ = -1;
  std::FILE* takeFile_ = nullptr;
  bool keepTakes_ = false;
};

}  // namespace synth
