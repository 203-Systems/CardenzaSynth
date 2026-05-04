#pragma once

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::System {

    esp_err_t Init();
    void EnterDeepSleep();

    i2c_master_bus_handle_t GetI2CBus();

}  // namespace ComputeCardHAL::System
