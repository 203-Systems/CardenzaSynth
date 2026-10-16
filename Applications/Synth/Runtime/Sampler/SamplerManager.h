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
#include <memory>

namespace synth {

class SamplerManager {
public:
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

  SamplerTrackUiState BuildUiState(std::size_t trackIndex) const;
  bool ConsumeTrimReset(std::size_t trackIndex);
  static bool ValidateSampleReference(std::string_view id);
  static bool PrepareSample(std::string_view id, PreparedSample& sample);
  bool ApplySample(std::size_t trackIndex, const PreparedSample& sample);

private:
  struct TrackState {
    SamplerTrackUiState ui;
    std::shared_ptr<audio::SampleBuffer> draftFrames;
    audio::SharedSampleBuffer currentFrames;
    std::uint32_t currentSampleRate = audio::kSampleRate;
    std::string pendingDeleteFileName;
    bool trimResetPending = false;
    bool isSampler = false;
    std::uint8_t previewNote = 60;
    std::chrono::steady_clock::time_point previewTickAt{};
  };

  bool IsValidSamplerTrack(std::size_t trackIndex) const;
  bool CanReplaceSample(std::size_t trackIndex);
  bool FileUsedByOtherTrack(std::size_t trackIndex, std::string_view fileName) const;
  static audio::SharedSampleBuffer ActiveFrames(const TrackState& state);
  void StopPreview(std::size_t trackIndex, TrackState& state);
  void SyncAudioSample(std::size_t trackIndex, const TrackState& state);
  void PumpInput(TrackState& state);
  void FinishDraft(TrackState& state);
  bool SaveDraftAs(TrackState& state, std::string_view name, bool overwrite);

  std::array<TrackState, kSynthTrackCount> tracks_{};
  int activeRecordingTrack_ = -1;
};

}  // namespace synth
