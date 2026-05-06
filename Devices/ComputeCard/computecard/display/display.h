#pragma once

#include <cstdint>

namespace ComputeCardHAL::Display {
    constexpr int WIDTH = 240;
    constexpr int HEIGHT = 135;

    bool Init();
    bool PresentFrame(const std::uint16_t* pixels, int width, int height);

    bool SetBrightness(uint8_t brightness);
    uint8_t GetBrightness();
}
