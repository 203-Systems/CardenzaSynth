#include "OS/Framework/Midi/MidiPacket.h"

namespace midi {

MidiPacket::MidiPacket(MidiStatus statusValue, int data0, int data1, int data2)
    : status(statusValue),
      data{static_cast<uint8_t>(data0), static_cast<uint8_t>(data1), static_cast<uint8_t>(data2)} {}

MidiPacket MidiPacket::NoteOn(uint8_t channel, uint8_t note, uint8_t velocity) {
  return MidiPacket(MidiStatus::NoteOn, channel & 0x0F, note, velocity);
}

MidiPacket MidiPacket::NoteOff(uint8_t channel, uint8_t note, uint8_t velocity) {
  return MidiPacket(MidiStatus::NoteOff, channel & 0x0F, note, velocity);
}

MidiPacket MidiPacket::ControlChange(uint8_t channel, uint8_t controller, uint8_t value) {
  return MidiPacket(MidiStatus::ControlChange, channel & 0x0F, controller, value);
}

MidiPacket MidiPacket::ProgramChange(uint8_t channel, uint8_t program) {
  return MidiPacket(MidiStatus::ProgramChange, channel & 0x0F, program, 0);
}

uint16_t MidiPacket::Port() const {
  return port;
}

void MidiPacket::SetPort(uint16_t portId) {
  port = portId;
}

uint8_t MidiPacket::Channel() const {
  return data[0] & 0x0F;
}

uint16_t MidiPacket::Value() const {
  if (status == MidiStatus::PitchChange || status == MidiStatus::SongPosition)
  {
    return static_cast<uint16_t>((data[2] << 7) | data[1]);
  }
  return data[2];
}

uint8_t MidiPacket::Length() const {
  switch (status)
  {
    case MidiStatus::ProgramChange:
    case MidiStatus::ChannelPressure:
      return 2;
    case MidiStatus::TuneRequest:
    case MidiStatus::Clock:
    case MidiStatus::Tick:
    case MidiStatus::Start:
    case MidiStatus::Continue:
    case MidiStatus::Stop:
    case MidiStatus::ActiveSense:
    case MidiStatus::Reset:
      return 1;
    default:
      return 3;
  }
}

bool MidiPacket::IsChannelMessage() const {
  const auto rawStatus = static_cast<uint8_t>(status);
  return rawStatus >= 0x80 && rawStatus <= 0xEF;
}

}  // namespace midi
