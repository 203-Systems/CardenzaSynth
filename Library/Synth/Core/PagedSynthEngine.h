#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "Library/Synth/Core/ISynthEngine.h"

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
  SynthEngineState CaptureState() const override;
  SynthEngineState CaptureChanges() const override;
  bool RestoreState(const SynthEngineState& state) override;
  bool CanRestore(const SynthEngineState& state) const override;
  bool RefreshPresetCatalog() override;
  bool CanReplace(const ISynthEngine& previous) const override;
  void ReplaceBinding(ISynthEngine& previous) override;
  std::string DebugState() const override;

protected:
  // A preset's values by parameter index, sorted: four bytes a value instead of a map node of about
  // fifty (a DX7 user preset is 0.4 KB, not 5 KB), because an engine keeps its whole bank in RAM.
  struct PresetValue {
    std::uint16_t parameter;
    std::int16_t value;
  };
  struct PresetState {
    SynthPresetInfo info;
    std::vector<PresetValue> values;
    std::string basePresetId;
  };

  // An engine's constructor states how many parameters it registers (its own count; the track effects
  // are added). The list is kept in small blocks (a deque): as one block a DX7's is 9.5 KB, which a
  // fragmented heap on the device often does not have, while everything else would fit (2026-11-03).
  void ReserveParameters(std::size_t engineParameterCount);
  void RegisterParameter(const SynthParameterInfo& parameter);
  // The track effect parameters every engine carries (see Library/Synth/Audio/SynthFx.h), registered
  // last.
  static constexpr std::size_t kTrackFxParameterCount = 7;
  void RegisterTrackFxParameters();
  void RegisterPreset(const std::string& presetId,
                      const std::string& presetName,
                      const std::map<std::string, int>& values,
                      bool writable = false);
  // Large factory catalogs stay in flash: derived engines expose them here and call
  // IndexFactoryPresets() once; RAM keeps only the 16-bit sorted order.
  void IndexFactoryPresets();
  virtual std::size_t FactoryPresetCount() const;
  virtual std::string_view FactoryPresetId(std::size_t index) const;
  virtual std::string FactoryPresetName(std::size_t index) const;
  SynthParameterInfo* FindParameterMutable(const std::string& parameterId);
  const SynthParameterInfo* FindParameter(const std::string& parameterId) const;
  // Index of a parameter of this engine (the list is fixed after the constructor).
  std::size_t ParameterIndex(const SynthParameterInfo& parameter) const;
  // Values by parameter ID (the file and project form) into the compact form; false for an unknown ID.
  bool CompactValues(const std::map<std::string, int>& values, std::vector<PresetValue>& compact) const;
  static const PresetValue* FindPresetValue(const PresetState& preset, std::size_t parameterIndex);

  virtual std::string FormatParameterValue(const SynthParameterInfo& parameter) const;
  virtual void OnParameterChanged(const SynthParameterInfo& parameter);
  virtual void OnPresetLoaded(const std::string& presetId);
  virtual bool ShouldSyncParameterToRuntime(const SynthParameterInfo& parameter) const;
  virtual bool ShouldSyncParameterOnPresetLoad(const SynthParameterInfo& parameter) const;
  virtual int FactoryParameterValue(const std::string& presetId, const SynthParameterInfo& parameter) const;

private:
  static constexpr std::size_t kNoFactoryPreset = static_cast<std::size_t>(-1);

  void SyncRuntimeState();
  std::size_t FindFactoryPreset(const std::string& presetId) const;
  std::string FactoryPresetDisplayName(std::size_t index) const;
  std::string NextUserPresetId() const;
  bool ValidState(const SynthEngineState& state) const;
  bool ReadUserPresets();
  bool HasBinding() const override;
  RuntimeBinding TakeBinding() override;

  SynthDescriptor descriptor_;
  std::deque<SynthParameterInfo> parameters_{};
  std::size_t declaredParameters_ = 0;
  std::vector<std::uint16_t> factoryOrder_{};
  std::vector<PresetState> presets_{};
  std::unique_ptr<midi::MidiPort> port_;
  std::string currentPresetId_;
  std::string basePresetId_;
  std::string currentPresetName_ = "INIT";
  std::string lastEvent_ = "idle";
  std::string audioBackendName_ = "none";
  bool audioConnected_ = false;
  bool audioHasNativeOutput_ = false;
  int activeNotes_ = 0;
  std::size_t engineIndex_ = static_cast<std::size_t>(-1);
};

}  // namespace synth
