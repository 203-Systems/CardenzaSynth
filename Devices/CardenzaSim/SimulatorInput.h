#pragma once

#include <cstddef>
#include <cstdint>

namespace CardenzaSim::Input {

void SetMatrixKey(std::size_t row, std::size_t column, bool pressed);
void SetHomeButton(bool pressed);
void ClearMatrixKeys();
// Simulates taking the SD card out (false) and putting it back (true, the default).
void SetCardInserted(bool inserted);
// A microphone for hosts without capture hardware: StartInput succeeds and frames of a test tone
// become readable as they are fed.
void SetMicrophonePresent(bool present);
void FeedMicrophone(std::size_t frames);

// A computer on the USB port (Device::USB): plug it in or out, send the device MIDI packets, and take
// what the device sent back.
void SetComputerConnected(bool connected);
void SendComputerMidi(const std::uint8_t packet[4]);
bool TakeDeviceMidi(std::uint8_t packet[4]);

}  // namespace CardenzaSim::Input
