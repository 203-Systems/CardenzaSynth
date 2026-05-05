#pragma once

#include <cstdint>

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::Audio::Codec::ES8156 {

esp_err_t Init();
esp_err_t SetEnabled(bool enabled);
bool IsEnabled();

esp_err_t SetVolume(uint8_t volume);
uint8_t GetVolume();

}  // namespace ComputeCardHAL::Audio::Codec::ES8156