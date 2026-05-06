#pragma once

#include <cstdint>

namespace ComputeCardHAL::Storage {
    bool Init();
    bool IsMounted();
    uint64_t GetCapacity();
}
