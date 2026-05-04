#pragma once

#include <string>

#include "computecard/computecardHAL.h"

namespace HAL
{
    class Hal
    {
    public:
        virtual ~Hal() = default;
        virtual std::string type() { return "null"; }
        virtual esp_err_t init() { return ComputeCardHAL::Init(); }
    };
} // namespace HAL
