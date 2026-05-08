#include "Applications/Synth/Runtime/SynthRack.h"

#include <sstream>

namespace synth {

SynthRack::SynthRack() {
  for (std::size_t index = 0; index < slots_.size(); ++index)
  {
    slots_[index].trackIndex = index;
  }
}

bool SynthRack::AttachEngine(std::size_t trackIndex, SynthEngineKind kind) {
  if (trackIndex >= slots_.size())
  {
    return false;
  }

  auto& slot = slots_[trackIndex];
  slot.kind = kind;
  slot.engine = CreateSynthEngine(kind);

  if (!slot.engine)
  {
    return kind == SynthEngineKind::None;
  }

  const uint16_t portId = static_cast<uint16_t>(midi::MIDI_PORT_SYNTH + static_cast<uint16_t>(trackIndex));
  return slot.engine->Init({trackIndex, portId, "SynthTrack" + std::to_string(trackIndex + 1)});
}

void SynthRack::DispatchPendingMessages() {
  for (auto& slot : slots_)
  {
    if (!slot.engine)
    {
      continue;
    }

    slot.engine->DispatchPendingMessages();
  }
}

uint16_t SynthRack::PortForTrack(std::size_t trackIndex) const {
  if (trackIndex >= slots_.size() || !slots_[trackIndex].engine)
  {
    return midi::MIDI_PORT_INVALID;
  }
  return slots_[trackIndex].engine->PortId();
}

SynthSlot& SynthRack::Slot(std::size_t trackIndex) {
  return slots_.at(trackIndex);
}

const SynthSlot& SynthRack::Slot(std::size_t trackIndex) const {
  return slots_.at(trackIndex);
}

std::vector<std::string> SynthRack::DescribeSlots() const {
  std::vector<std::string> lines;
  lines.reserve(slots_.size());
  for (const auto& slot : slots_)
  {
    std::ostringstream stream;
    stream << "Track " << (slot.trackIndex + 1) << ": " << ToString(slot.kind);
    if (slot.engine && slot.engine->PortId() != midi::MIDI_PORT_INVALID)
    {
      stream << " @0x" << std::hex << std::uppercase << slot.engine->PortId() << std::dec;
    }
    if (slot.engine)
    {
      stream << "  " << slot.engine->DebugState();
    }
    lines.push_back(stream.str());
  }
  return lines;
}

}  // namespace synth
