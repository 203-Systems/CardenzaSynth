#pragma once

#include <cstdint>

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::Audio {
    enum class OutputMode : uint8_t {
        Off,
        Speaker,
    };

    esp_err_t Init();

    i2s_chan_handle_t GetTxChannel();
    i2s_chan_handle_t GetAudioChannel();

    esp_err_t SetVolume(uint8_t volume);
    uint8_t GetVolume();

    esp_err_t SetOutputMode(OutputMode mode);
    OutputMode GetOutputMode();
}  // namespace ComputeCardHAL::Audio
