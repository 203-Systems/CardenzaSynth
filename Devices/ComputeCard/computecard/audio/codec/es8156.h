#pragma once

#include <cstdint>

namespace ComputeCardHAL::Audio::Codec::ES8156 {

bool Init();
bool SetEnabled(bool enabled);
bool IsEnabled();

bool SetVolume(uint8_t volume);
uint8_t GetVolume();

}  // namespace ComputeCardHAL::Audio::Codec::ES8156