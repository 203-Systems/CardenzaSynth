#include "OS/MIDI/MIDI.h"

#include <memory>

namespace os::midi {

namespace {

std::unique_ptr<::midi::MidiPort>& OsPortStorage() {
  static std::unique_ptr<::midi::MidiPort> osPort;
  return osPort;
}

}  // namespace

void Initialize() {
  if (!OsPortStorage())
  {
    OsPortStorage() = std::make_unique<::midi::MidiPort>("CardenzaSynth", ::midi::MIDI_PORT_OS, 256);
  }
}

::midi::MidiPort& OsPort() {
  Initialize();
  return *OsPortStorage();
}

bool Send(const ::midi::MidiPacket& packet, uint16_t targetPort) {
  Initialize();
  return OsPortStorage()->Send(packet, targetPort);
}

}  // namespace os::midi
