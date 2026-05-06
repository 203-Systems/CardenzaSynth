#include "system.h"
#include "../board/pins.h"

#if defined(ESP_PLATFORM)
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#endif

namespace ComputeCardHAL::System {

namespace {

#if defined(ESP_PLATFORM)
i2c_master_bus_handle_t i2cBus = nullptr;
#endif

}  // namespace

bool Init()
{
#if defined(ESP_PLATFORM)
    if (i2cBus != nullptr)
    {
        return true;
    }

    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = static_cast<i2c_port_num_t>(Pins::kExternalI2cPort);
    busConfig.scl_io_num = static_cast<gpio_num_t>(Pins::kExternalI2cScl);
    busConfig.sda_io_num = static_cast<gpio_num_t>(Pins::kExternalI2cSda);
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = false;

    return i2c_new_master_bus(&busConfig, &i2cBus) == ESP_OK;
#else
    return false;
#endif
}

void EnterDeepSleep()
{
#if defined(ESP_PLATFORM)
    if (i2cBus != nullptr)
    {
        i2c_del_master_bus(i2cBus);
        i2cBus = nullptr;
    }
#endif
}

#if defined(ESP_PLATFORM)
i2c_master_bus_handle_t GetI2CBus()
{
    return i2cBus;
}
#endif

}  // namespace ComputeCardHAL::System