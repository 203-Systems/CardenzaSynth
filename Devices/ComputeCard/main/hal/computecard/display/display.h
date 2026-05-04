#pragma once

#include <cstdint>

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::Display {
    constexpr int WIDTH = 240;
    constexpr int HEIGHT = 135;

    esp_err_t Init();
    esp_lcd_panel_handle_t GetPanel();
    esp_lcd_panel_io_handle_t GetPanelIO();

    esp_err_t SetBrightness(uint8_t brightness);
    uint8_t GetBrightness();
}
