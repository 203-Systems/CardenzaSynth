#pragma once

#include <cstdint>

// What has gone through the USB audio so far, for the device layer's answers to test requests from a
// computer (Device.cpp). The peak starts again with each call.
namespace Device::Audio {

struct ComputerAudioStats {
  std::uint32_t framesToComputer = 0;
  std::uint32_t framesFromComputer = 0;
  std::uint16_t peakFromComputer = 0;
};

ComputerAudioStats ComputerStats();

}  // namespace Device::Audio
