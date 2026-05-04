#pragma once

#include "Devices/ComputeCard/main/hal/computecard/espTypes.h"

#include "Devices/ComputeCard/main/hal/computecard/audio/audio.h"
#include "Devices/ComputeCard/main/hal/computecard/display/display.h"
#include "Devices/ComputeCard/main/hal/computecard/ir/ir.h"
#include "Devices/ComputeCard/main/hal/computecard/keyboard/keyboard.h"
#include "Devices/ComputeCard/main/hal/computecard/mic/mic.h"
#include "Devices/ComputeCard/main/hal/computecard/storage/storage.h"
#include "Devices/ComputeCard/main/hal/computecard/system/system.h"


namespace ComputeCardHAL {
esp_err_t Init();
}  // namespace ComputeCardHAL