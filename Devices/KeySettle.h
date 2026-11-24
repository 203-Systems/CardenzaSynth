#pragma once

// CardenzaHAL's key settle filter, pure (no ESP-IDF). Devices/Cardenza/Device.cpp uses it on the left
// column (ESC, TAB, FN, CTRL): a state must hold for 12 ms, and a key that reads as held for over 15 s
// counts as released until it lets go. The class is the HAL's (keyboard/key_settle.h); this is the name
// this firmware has always used for it.
#include "Devices/Cardenza/CardenzaHAL/keyboard/key_settle.h"

namespace Device::Keyboard {

template <std::size_t kKeys>
using KeySettle = CardenzaHAL::Keyboard::KeySettle<kKeys>;

}  // namespace Device::Keyboard
