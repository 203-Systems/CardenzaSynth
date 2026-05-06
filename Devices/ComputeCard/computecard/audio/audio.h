#pragma once

#include <cstddef>
#include <cstdint>

namespace ComputeCardHAL::Audio {
    enum class OutputMode : uint8_t {
        Off,
        Speaker,
    };

    bool Init();
    bool NotifyOutputClockStarted();

    bool IsOutputReady();
    bool EnableOutput();
    bool WriteOutputFrames(const std::int16_t* samples, std::size_t sampleCount);

    bool SetVolume(uint8_t volume);
    uint8_t GetVolume();

    bool SetOutputMode(OutputMode mode);
    OutputMode GetOutputMode();
}  // namespace ComputeCardHAL::Audio
