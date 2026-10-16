#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "Library/Synth/Core/SynthTypes.h"
#include "OS/Framework/Midi/MidiPacket.h"
#include "OS/Framework/Midi/MidiPort.h"

namespace synth {

struct SynthDescriptor {
  SynthEngineKind kind = SynthEngineKind::None;
  std::string id;
  std::string displayName;
};

struct SynthEngineInit {
  std::size_t engineIndex = 0;
  uint16_t portId = midi::MIDI_PORT_INVALID;
  std::string portName;
};

enum class SynthParameterKind {
  Continuous,
  Toggle,
  Choice,
};

struct SynthPageRequest {
  std::size_t offset = 0;
  std::size_t limit = 8;
};

struct SynthParameterInfo {
  std::string id;
  std::string label;
  std::string group;
  SynthParameterKind kind = SynthParameterKind::Continuous;
  int minValue = 0;
  int maxValue = 127;
  int step = 1;
  int defaultValue = 0;
  int value = 0;
};

struct SynthParameterPage {
  std::size_t totalCount = 0;
  std::vector<SynthParameterInfo> items;
};

struct SynthPresetInfo {
  std::string id;
  std::string name;
  bool writable = false;
  bool active = false;
};

struct SynthPresetPage {
  std::size_t totalCount = 0;
  std::vector<SynthPresetInfo> items;
};

struct SynthEngineState {
  std::string basePresetId;
  std::string presetId;
  std::string presetName;
  std::map<std::string, int> values;
};

class ISynthEngine {
public:
  virtual ~ISynthEngine() = default;

  virtual bool Init(const SynthEngineInit& init) = 0;
  virtual uint16_t PortId() const = 0;
  virtual void DispatchPendingMessages() = 0;
  virtual const SynthDescriptor& Descriptor() const = 0;
  virtual SynthParameterPage ListParameters(const SynthPageRequest& request) const = 0;
  virtual SynthPresetPage ListPresets(const SynthPageRequest& request) const = 0;
  virtual bool GetParameterInfo(const std::string& parameterId, SynthParameterInfo& parameter) const = 0;
  virtual void ProcessMidi(const midi::MidiPacket& packet) = 0;
  virtual bool SetParameterValue(const std::string& parameterId, int value) = 0;
  virtual bool AdjustParameterValue(const std::string& parameterId, int delta) = 0;
  virtual std::string DescribeParameterValue(const std::string& parameterId) const = 0;
  virtual bool LoadPreset(const std::string& presetId) = 0;
  virtual bool SavePreset(const std::string& presetId, const std::string& presetName = "") = 0;
  virtual std::string CurrentPresetId() const = 0;
  virtual std::string CurrentPresetName() const = 0;
  virtual SynthEngineState CaptureState() const = 0;
  virtual bool RestoreState(const SynthEngineState& state) = 0;
  virtual bool RefreshPresetCatalog() = 0;
  virtual bool CanReplace(const ISynthEngine& previous) const = 0;
  virtual void ReplaceBinding(ISynthEngine& previous) = 0;
  virtual std::string DebugState() const = 0;

protected:
  struct RuntimeBinding {
    std::unique_ptr<midi::MidiPort> port;
    std::size_t engineIndex;
    std::string backendName;
    bool connected;
    bool nativeOutput;
  };

private:
  friend class PagedSynthEngine;
  virtual bool HasBinding() const = 0;
  virtual RuntimeBinding TakeBinding() = 0;
};

std::unique_ptr<ISynthEngine> CreateSynthEngine(SynthEngineKind kind);
std::string ToString(SynthEngineKind kind);

}  // namespace synth
