#include "audio.h"
#include "codec/es8156.h"
#include "../board/pins.h"
#include "../system/system.h"
#include "../system/system_internal.h"

#if !defined(ESP_PLATFORM)
#error "ComputeCard audio HAL is ESP-IDF only. Use ComputeCardSim for host builds."
#endif

#include <esp_log.h>
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>

namespace ComputeCardHAL::Audio {

namespace {

i2s_chan_handle_t txChannel = nullptr;
OutputStatus outputStatus;

constexpr char kLogTag[] = "ComputeCardAudio";

constexpr i2s_port_t kAudioPort = I2S_NUM_1;
constexpr uint32_t kSampleRate = 44100;
constexpr size_t kDmaFrameCount = 256;
constexpr size_t kDmaDescriptorCount = 8;

esp_err_t EnsureI2sInitialized()
{
    if (txChannel != nullptr)
    {
        outputStatus.i2sReady = true;
        return ESP_OK;
    }

    i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(kAudioPort, I2S_ROLE_MASTER);
    channelConfig.dma_desc_num = kDmaDescriptorCount;
    channelConfig.dma_frame_num = kDmaFrameCount;
    channelConfig.auto_clear = true;

    esp_err_t status = i2s_new_channel(&channelConfig, &txChannel, nullptr);
    if (status != ESP_OK)
    {
        txChannel = nullptr;
        outputStatus.i2sReady = false;
        return status;
    }

    i2s_std_config_t standardConfig = {
        .clk_cfg = {
            .sample_rate_hz = kSampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = static_cast<gpio_num_t>(Pins::kSpeakerBck),
            .ws = static_cast<gpio_num_t>(Pins::kSpeakerWs),
            .dout = static_cast<gpio_num_t>(Pins::kSpeakerDataOut),
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    standardConfig.slot_cfg.slot_bit_width = I2S_SLOT_BIT_WIDTH_32BIT;
    standardConfig.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    status = i2s_channel_init_std_mode(txChannel, &standardConfig);
    if (status != ESP_OK)
    {
        i2s_del_channel(txChannel);
        txChannel = nullptr;
        outputStatus.i2sReady = false;
        outputStatus.outputEnabled = false;
    }
    else
    {
        outputStatus.i2sReady = true;
    }

    return status;
}

bool EnsureCodecReady()
{
    if (outputStatus.codec == CodecStatus::Available)
    {
        return true;
    }

    if (outputStatus.codec == CodecStatus::Unavailable)
    {
        return false;
    }

    if (ComputeCardHAL::System::GetI2CBus() == nullptr)
    {
        if (!ComputeCardHAL::System::Init())
        {
            ESP_LOGE(kLogTag, "Failed to initialize I2C bus for audio codec");
            outputStatus.codec = CodecStatus::Unavailable;
            return false;
        }
    }

    if (!Codec::ES8156::Init())
    {
        ESP_LOGW(kLogTag, "ES8156 codec unavailable, continuing with I2S only");
        outputStatus.codec = CodecStatus::Unavailable;
        return false;
    }

    outputStatus.codec = CodecStatus::Available;
    return true;
}

bool ApplyCachedOutputMode()
{
    if (outputStatus.codec != CodecStatus::Available)
    {
        return true;
    }

    return Codec::ES8156::SetEnabled(outputStatus.mode == OutputMode::Speaker);
}

bool ApplyCachedVolume()
{
    if (outputStatus.codec != CodecStatus::Available)
    {
        return true;
    }

    return Codec::ES8156::SetVolume(outputStatus.volume);
}

}  // namespace

bool Init()
{
    return EnsureI2sInitialized() == ESP_OK;
}

bool NotifyOutputClockStarted()
{
    if (!EnsureCodecReady())
    {
        // The board can still stream I2S even when codec control is unavailable.
        outputStatus.codec = CodecStatus::Unavailable;
    }

    if (!ApplyCachedOutputMode())
    {
        outputStatus.codec = CodecStatus::Unavailable;
    }

    if (!ApplyCachedVolume())
    {
        outputStatus.codec = CodecStatus::Unavailable;
    }
    return true;
}

bool IsOutputReady()
{
    return Init() && outputStatus.i2sReady && txChannel != nullptr;
}

OutputStatus GetOutputStatus()
{
    return outputStatus;
}

bool EnableOutput()
{
    if (!IsOutputReady())
    {
        return false;
    }

    if (outputStatus.outputEnabled)
    {
        return true;
    }

    if (i2s_channel_enable(txChannel) != ESP_OK)
    {
        outputStatus.outputEnabled = false;
        return false;
    }

    outputStatus.outputEnabled = true;
    return true;
}

bool WriteOutputFrames(const std::int16_t* samples, std::size_t sampleCount)
{
    if (!EnableOutput() || samples == nullptr || sampleCount == 0)
    {
        return false;
    }

    std::size_t bytesWritten = 0;
    return i2s_channel_write(txChannel,
                             samples,
                             sampleCount * sizeof(std::int16_t),
                             &bytesWritten,
                             portMAX_DELAY) == ESP_OK;
}

bool SetVolume(uint8_t value)
{
    if (value > 100)
    {
        value = 100;
    }

    outputStatus.volume = value;
    return ApplyCachedVolume();
}

uint8_t GetVolume()
{
    return outputStatus.volume;
}

bool SetOutputMode(OutputMode mode)
{
    outputStatus.mode = mode;
    return ApplyCachedOutputMode();
}

OutputMode GetOutputMode()
{
    return outputStatus.mode;
}

}  // namespace ComputeCardHAL::Audio
