#pragma once

#include <cstdint>

namespace midi {

enum class MidiStatus : uint8_t {
  None = 0x00,
  NoteOff = 0x80,
  NoteOn = 0x90,
  AfterTouch = 0xA0,
  ControlChange = 0xB0,
  ProgramChange = 0xC0,
  ChannelPressure = 0xD0,
  PitchChange = 0xE0,
  SysExData = 0xF0,
  SongPosition = 0xF2,
  SongSelect = 0xF3,
  TuneRequest = 0xF6,
  SysExEnd = 0xF7,
  Clock = 0xF8,
  Tick = 0xF9,
  Start = 0xFA,
  Continue = 0xFB,
  Stop = 0xFC,
  ActiveSense = 0xFE,
  Reset = 0xFF,
};

enum MidiPortId : uint16_t {
  MIDI_PORT_EACH_CLASS = 0x0000,
  MIDI_PORT_ALL = 0x0001,
  MIDI_PORT_USB = 0x0100,
  MIDI_PORT_PHYSICAL = 0x0200,
  MIDI_PORT_BLUETOOTH = 0x0300,
  MIDI_PORT_WIRELESS = 0x0400,
  MIDI_PORT_RTP = 0x0500,
  MIDI_PORT_DEVICE_CUSTOM = 0x0600,
  MIDI_PORT_SYNTH = 0x8000,
  MIDI_PORT_OS = 0xF000,
  MIDI_PORT_INVALID = 0xFFFF,
};

struct MidiPacket {
  uint16_t port = MIDI_PORT_INVALID;
  MidiStatus status = MidiStatus::None;
  uint8_t data[3] = {0, 0, 0};

  MidiPacket() = default;
  MidiPacket(MidiStatus statusValue, int data0 = 0, int data1 = 0, int data2 = 0);

  static MidiPacket NoteOn(uint8_t channel, uint8_t note, uint8_t velocity = 127);
  static MidiPacket NoteOff(uint8_t channel, uint8_t note, uint8_t velocity = 0);
  static MidiPacket ControlChange(uint8_t channel, uint8_t controller, uint8_t value);
  static MidiPacket ProgramChange(uint8_t channel, uint8_t program);

  uint16_t Port() const;
  void SetPort(uint16_t portId);
  uint8_t Channel() const;
  uint16_t Value() const;
  uint8_t Length() const;
  bool IsChannelMessage() const;
};

}  // namespace midi
