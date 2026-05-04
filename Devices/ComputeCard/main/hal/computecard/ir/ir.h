#pragma once

#include <cstdint>

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::IR {

    esp_err_t Init();
    void Send(uint8_t addr, uint8_t cmd);

}  // namespace ComputeCardHAL::IR
