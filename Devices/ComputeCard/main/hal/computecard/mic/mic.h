#pragma once

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::Mic {

    esp_err_t Init();
    i2s_chan_handle_t GetMicChannel();

}  // namespace ComputeCardHAL::Mic
