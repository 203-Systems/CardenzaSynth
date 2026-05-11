#pragma once

#include "Applications/Synth/Runtime/Sampler/SamplerTrackState.h"
#include "Library/Synth/Core/SynthTypes.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace synth {

class SamplerManager {
public:
  void OnTrackEngineChanged(std::size_t trackIndex, SynthEngineKind kind);
  void Tick();

  bool BeginRecord(std::size_t trackIndex);
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
  bool DeleteBrowserSample(std::size_t trackIndex, std::string_view id);
  bool RenameCurrentSample(std::size_t trackIndex, std::string_view newName);
  bool DeleteCurrentSample(std::size_t trackIndex);

  SamplerTrackUiState BuildUiState(std::size_t trackIndex) const;

private:
  struct TrackState {
    SamplerTrackUiState ui;
    std::vector<std::int16_t> draftFrames;
    std::vector<std::int16_t> currentFrames;
    bool isSampler = false;
    std::uint8_t previewNote = 60;
    std::chrono::steady_clock::time_point previewTickAt{};
  };

  bool IsValidSamplerTrack(std::size_t trackIndex) const;
  static const std::vector<std::int16_t>& ActiveFrames(const TrackState& state);
  void StopPreview(std::size_t trackIndex, TrackState& state);
  void SyncAudioSample(std::size_t trackIndex, const TrackState& state);
  void PumpInput(TrackState& state);
  void FinishDraft(TrackState& state);
  bool SaveDraftAs(TrackState& state, std::string_view name, bool overwrite);

  std::array<TrackState, kSynthTrackCount> tracks_{};
  int activeRecordingTrack_ = -1;
};

}  // namespace synth