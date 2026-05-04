#pragma once

#include <cstdint>

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

namespace ComputeCardHAL::Storage {
    esp_err_t Init();
    bool IsMounted();
    uint64_t GetCapacity();
    sdmmc_card_t* GetCard();
}
