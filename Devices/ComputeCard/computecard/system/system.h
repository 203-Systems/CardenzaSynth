#pragma once

#include <driver/i2c_master.h>

namespace ComputeCardHAL::System {

    bool Init();
    void EnterDeepSleep();
    i2c_master_bus_handle_t GetI2CBus();

}  // namespace ComputeCardHAL::System
