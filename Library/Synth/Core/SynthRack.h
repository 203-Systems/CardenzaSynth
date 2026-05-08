#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Library/Synth/Core/ISynthEngine.h"

namespace synth {

struct SynthSlot {
  std::size_t trackIndex = 0;
  SynthEngineKind kind = SynthEngineKind::None;
  std::unique_ptr<ISynthEngine> engine;
};

class SynthRack {
public:
  SynthRack();

  bool AttachEngine(std::size_t trackIndex, SynthEngineKind kind);
  void DispatchPendingMessages();
  uint16_t PortForTrack(std::size_t trackIndex) const;

  SynthSlot& Slot(std::size_t trackIndex);
  const SynthSlot& Slot(std::size_t trackIndex) const;
  std::vector<std::string> DescribeSlots() const;

private:
  std::array<SynthSlot, kSynthTrackCount> slots_;
};

}  // namespace synth
