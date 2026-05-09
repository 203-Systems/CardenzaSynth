#pragma once

#include <driver/i2s_pdm.h>

namespace ComputeCardHAL::Mic {

    bool Init();
    i2s_chan_handle_t InputChannel();

}  // namespace ComputeCardHAL::Mic
