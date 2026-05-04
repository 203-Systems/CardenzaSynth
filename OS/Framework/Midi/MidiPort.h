#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>

#include "OS/Framework/Midi/MidiPacket.h"

namespace midi {

class MidiPort {
public:
  MidiPort() = default;
  MidiPort(std::string name, uint16_t id, std::size_t queueSize = 64);
  MidiPort(std::string name, MidiPortId portClass, std::size_t queueSize = 64);
  ~MidiPort();

  uint16_t Open(uint16_t id, std::size_t queueSize = 64, uint16_t idRange = 1);
  void Close();
  bool Get(MidiPacket* destination);
  bool Send(MidiPacket packet, uint16_t targetPort = MIDI_PORT_OS);
  bool Receive(MidiPacket packet);

  const std::string& Name() const;
  void SetName(std::string name);
  uint16_t Id() const;

  static bool OpenMidiPort(uint16_t portId, MidiPort* port);
  static void CloseMidiPort(uint16_t portId);
  static bool RouteMidiPacket(MidiPacket packet, uint16_t targetPort);

private:
  static std::map<uint16_t, MidiPort*>& Registry();
  static std::mutex& RegistryMutex();

  std::string name_;
  uint16_t id_ = MIDI_PORT_INVALID;
  std::size_t queueSize_ = 64;
  std::deque<MidiPacket> queue_;
  std::mutex queueMutex_;
};

}  // namespace midi
