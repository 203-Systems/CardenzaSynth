#include "system.h"
#include "../board/pins.h"

#include <driver/i2c_master.h>
#include <driver/gpio.h>

namespace ComputeCardHAL::System {

namespace {

i2c_master_bus_handle_t i2cBus = nullptr;

}  // namespace

bool Init()
{
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
    busConfig.flags.enable_internal_pullup = true;

    return i2c_new_master_bus(&busConfig, &i2cBus) == ESP_OK;
}

void EnterDeepSleep()
{
    if (i2cBus != nullptr)
    {
        i2c_del_master_bus(i2cBus);
        i2cBus = nullptr;
    }
}

i2c_master_bus_handle_t GetI2CBus()
{
    return i2cBus;
}

}  // namespace ComputeCardHAL::System
