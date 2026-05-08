#pragma once

#include <array>
#include <string>
#include <vector>

#include "Applications/Application.h"
#include "Library/Synth/Core/SynthRack.h"
#include "Library/Synth/SynthUiModel.h"

namespace apps {

class SynthApp : public app::Application {
public:
  void Setup(const std::vector<std::string>& args) override;
  void Tick() override;
  void UpdateLeds() override;
  void RenderFrame() override;
  void HandleMatrixKeyEvent(std::size_t row, std::size_t col, bool pressed) override;

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
  synth::ISynthEngine* ActiveEngineMutable();
  const synth::ISynthEngine* ActiveEngine() const;
  void SwitchTrackEngine(std::size_t trackIndex, synth::SynthEngineKind kind);
  void SyncUiTracks();
  void ReleaseTrackPerformanceKeys(std::size_t trackIndex);
  void PressPerformanceKey(std::size_t row, std::size_t col);
  void ReleasePerformanceKey(std::size_t row, std::size_t col);

  bool initialized_ = false;
  synth::SynthRack rack_;
  synth::SynthUiModel ui_;
  std::array<std::array<bool, synth::kKeyboardMatrixRows * synth::kKeyboardMatrixColumns>, synth::kSynthTrackCount> activeKeys_{};
};

void RegisterSynthApplication();

}  // namespace apps
