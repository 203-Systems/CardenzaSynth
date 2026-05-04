#include "Devices/ComputeCard/main/hal/computecard/system/system.h"
#include "Devices/ComputeCard/main/hal/computecard/board/pins.h"

#if __has_include(<driver/i2c_master.h>)
#include <driver/i2c_master.h>
#include <driver/gpio.h>
#endif

namespace ComputeCardHAL::System {

namespace {

bool initialized = false;
i2c_master_bus_handle_t i2cBus = nullptr;

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/i2c_master.h>)
    if (i2cBus != nullptr)
    {
        initialized = true;
        return ESP_OK;
    }

    i2c_master_bus_config_t busConfig = {};
    busConfig.i2c_port = static_cast<i2c_port_num_t>(Pins::kExternalI2cPort);
    busConfig.scl_io_num = static_cast<gpio_num_t>(Pins::kExternalI2cScl);
    busConfig.sda_io_num = static_cast<gpio_num_t>(Pins::kExternalI2cSda);
    busConfig.clk_source = I2C_CLK_SRC_DEFAULT;
    busConfig.glitch_ignore_cnt = 7;
    busConfig.flags.enable_internal_pullup = true;

    const esp_err_t status = i2c_new_master_bus(&busConfig, &i2cBus);
    initialized = (status == ESP_OK);
    return status;
#else
    initialized = false;
    return ESP_FAIL;
#endif
}

void EnterDeepSleep()
{
#if __has_include(<driver/i2c_master.h>)
    if (i2cBus != nullptr)
    {
        i2c_del_master_bus(i2cBus);
        i2cBus = nullptr;
    }
#endif
    initialized = false;
}

i2c_master_bus_handle_t GetI2CBus()
{
    return i2cBus;
}

}  // namespace ComputeCardHAL::System