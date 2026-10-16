#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Applications/Synth/Runtime/Sampler/SamplerManager.h"
#include "Applications/Synth/Runtime/Sequencer/SynthSequencer.h"
#include "Applications/Synth/Runtime/SynthRack.h"
#include "Applications/Synth/UI/SynthUiModel.h"

namespace apps {

class SynthApp : public app::Application {
public:
  void Setup(const std::vector<std::string>& args) override;
  void Tick() override;
  void UpdateLeds() override;
  void RenderFrame() override;
  void HandleMatrixKeyEvent(std::size_t row, std::size_t col, bool pressed) override;
  void HandleHomeButtonEvent(bool pressed) override;

  std::string DebugView() const;
  synth::MatrixKeyHandling PressMatrixKey(std::size_t row, std::size_t col);
  void ReleaseMatrixKey(std::size_t row, std::size_t col);
  bool IsKeyActive(std::size_t trackIndex, std::size_t keyIndex) const;
  const std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>& ActiveKeyStates(std::size_t trackIndex) const;

  synth::SynthRack& Rack() { return rack_; }
  const synth::SynthRack& Rack() const { return rack_; }

  synth::SynthUiModel& Ui() { return ui_; }
  const synth::SynthUiModel& Ui() const { return ui_; }

private:
  void ApplyPendingUiActions();
  void SaveProject(const std::string& name, bool overwrite);
  void ChangeProject(const std::string& name, bool createNew);
  void StopProjectPerformance();
  synth::ISynthEngine* ActiveEngineMutable();
  const synth::ISynthEngine* ActiveEngine() const;
  void SwitchTrackEngine(std::size_t trackIndex, synth::SynthEngineKind kind);
  void SyncUiTracks();
  void SyncSequencerUi();
  void ReleaseTrackPerformanceKeys(std::size_t trackIndex);
  std::size_t CollectHeldPerformanceNotes(std::size_t trackIndex, std::array<std::uint8_t, synth::kKeyboardMatrixRows * synth::kHomePerformanceColumnCount>& notes) const;
  bool IsSequencerShiftKey(std::size_t row, std::size_t col) const;
  bool IsSequencerShiftActive() const;
  bool IsSequencerShiftModifierActive() const;
  bool IsSequencerPatternSelectorKey(std::size_t row, std::size_t col) const;
  bool IsSequencerClearMode() const;
  bool IsSequencerCopyMode() const;
  bool IsSequencerQuantizeMode() const;
  void MarkSequencerShiftEvent();
  void ClearSequencerCopySource();
  void ClearSequencerHomeGesture();
  bool HandleSequencerShiftPress(std::size_t row, std::size_t col);
  bool HandleSequencerShiftRelease(std::size_t row, std::size_t col);
  bool HandleSequencerShiftActionPress(std::size_t row);
  bool HandleSequencerShiftActionRelease(std::size_t row);
  bool HandleSequencerPatternSelectorPress(std::size_t row, std::size_t col);
  bool HandleSequencerPatternSelectorRelease(std::size_t row, std::size_t col);
  bool HandleSequencerPress(std::size_t row, std::size_t col);
  bool HandleSequencerRelease(std::size_t row, std::size_t col);
  void PressPerformanceKey(std::size_t row, std::size_t col);
  void ReleasePerformanceKey(std::size_t row, std::size_t col);
  static void SendSequencerMidi(void* context, std::size_t trackIndex, const midi::MidiPacket& packet);

  bool initialized_ = false;
  synth::SynthRack rack_;
  synth::SamplerManager samplerManager_;
  synth::SynthSequencer sequencer_;
  synth::SynthUiModel ui_;
  enum class SequencerCopySourceType {
    None,
    Step,
    Pattern,
  };
  struct SequencerCopySource {
    SequencerCopySourceType type = SequencerCopySourceType::None;
    std::size_t index = 0;
  };
  std::array<bool, 2> sequencerShiftKeys_{};
  std::array<std::chrono::steady_clock::time_point, 2> sequencerShiftPressedAt_{};
  std::array<bool, synth::kKeyboardMatrixRows> sequencerShiftActionHeld_{};
  std::array<bool, synth::kSequencerMaxPatternCount> sequencerPatternPressed_{};
  bool sequencerShiftEventOccurred_ = false;
  bool sequencerViewChordActive_ = false;
  SequencerCopySource sequencerCopySource_{};
  std::array<std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>, synth::kSynthTrackCount> activeKeys_{};
  std::array<std::array<std::uint8_t, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>, synth::kSynthTrackCount> activeNotes_{};
};

void RegisterSynthApplication();

}  // namespace apps
