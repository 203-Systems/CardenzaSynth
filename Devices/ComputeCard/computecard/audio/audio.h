#pragma once

#include <cstddef>
#include <cstdint>

namespace ComputeCardHAL::Audio {
    enum class OutputMode : uint8_t {
        Off,
        Speaker,
    };

    enum class CodecStatus : uint8_t {
        NotProbed,
        Available,
        Unavailable,
    };

    struct OutputStatus {
        bool i2sReady = false;
        bool outputEnabled = false;
        CodecStatus codec = CodecStatus::NotProbed;
        OutputMode mode = OutputMode::Speaker;
        uint8_t volume = 100;
    };

    bool Init();
    bool NotifyOutputClockStarted();

    bool IsOutputReady();
    OutputStatus GetOutputStatus();
    bool EnableOutput();
    bool WriteOutputFrames(const std::int16_t* samples, std::size_t sampleCount);

    bool SetVolume(uint8_t volume);
    uint8_t GetVolume();

    bool SetOutputMode(OutputMode mode);
    OutputMode GetOutputMode();
}  // namespace ComputeCardHAL::Audio
