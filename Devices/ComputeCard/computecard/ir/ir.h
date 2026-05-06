#pragma once

#include <cstdint>

namespace ComputeCardHAL::IR {

    bool Init();
    void Send(uint8_t addr, uint8_t cmd);

}  // namespace ComputeCardHAL::IR
