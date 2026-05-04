#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Library/Synth/ISynthEngine.h"

namespace midi {
class MidiPort;
}

namespace synth {

class PagedSynthEngine : public ISynthEngine {
public:
  explicit PagedSynthEngine(SynthDescriptor descriptor);
  ~PagedSynthEngine() override;

  bool Init(const SynthEngineInit& init) override;
  uint16_t PortId() const override;
  void DispatchPendingMessages() override;
  const device::SynthAudioPipelineBinding& AudioBinding() const override;
  const SynthDescriptor& Descriptor() const override;
  SynthParameterPage ListParameters(const SynthPageRequest& request) const override;
  SynthPresetPage ListPresets(const SynthPageRequest& request) const override;
  bool GetParameterInfo(const std::string& parameterId, SynthParameterInfo& parameter) const override;
  void ProcessMidi(const midi::MidiPacket& packet) override;
  bool SetParameterValue(const std::string& parameterId, int value) override;
  bool AdjustParameterValue(const std::string& parameterId, int delta) override;
  std::string DescribeParameterValue(const std::string& parameterId) const override;
  bool LoadPreset(const std::string& presetId) override;
  bool SavePreset(const std::string& presetId, const std::string& presetName = "") override;
  std::string CurrentPresetId() const override;
  std::string CurrentPresetName() const override;
  std::string DebugState() const override;

protected:
  struct PresetState {
    SynthPresetInfo info;
    std::map<std::string, int> values;
  };

  void RegisterParameter(const SynthParameterInfo& parameter);
  void RegisterPreset(const std::string& presetId,
                      const std::string& presetName,
                      const std::map<std::string, int>& values,
                      bool writable = false);
  SynthParameterInfo* FindParameterMutable(const std::string& parameterId);
  const SynthParameterInfo* FindParameter(const std::string& parameterId) const;

  virtual std::string FormatParameterValue(const SynthParameterInfo& parameter) const;
  virtual void OnParameterChanged(const SynthParameterInfo& parameter);
  virtual void OnPresetLoaded(const std::string& presetId);

private:
  std::string NextUserPresetId() const;

  SynthDescriptor descriptor_;
  device::SynthAudioPipelineBinding audioBinding_;
  std::vector<SynthParameterInfo> parameters_{};
  std::vector<PresetState> presets_{};
  std::unique_ptr<midi::MidiPort> port_;
  std::string currentPresetId_;
  std::string currentPresetName_ = "INIT";
  std::string lastEvent_ = "idle";
  int activeNotes_ = 0;
};

}  // namespace synth