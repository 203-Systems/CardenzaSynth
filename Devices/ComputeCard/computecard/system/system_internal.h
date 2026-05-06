#pragma once

#if defined(ESP_PLATFORM)
#include <driver/i2c_master.h>

namespace ComputeCardHAL::System {

i2c_master_bus_handle_t GetI2CBus();

}  // namespace ComputeCardHAL::System

#endif