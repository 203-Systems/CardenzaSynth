#pragma once

#include "OS/Framework/Midi/MidiPort.h"

namespace os::midi {

void Initialize();
::midi::MidiPort& OsPort();
bool Send(const ::midi::MidiPacket& packet, uint16_t targetPort);

}  // namespace os::midi
