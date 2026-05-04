#pragma once

#include "hal.h"

namespace HAL
{
    class HalCardputer : public Hal
    {
        public:
            std::string type() override { return "computecard"; }
            esp_err_t init() override;
    };
}
