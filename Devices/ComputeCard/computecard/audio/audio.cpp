#include "audio.h"
#include "codec/es8156.h"
#include "../board/pins.h"
#include "../system/system.h"
#include "../system/system_internal.h"

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#endif

#if defined(ESP_PLATFORM)
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#include <freertos/FreeRTOS.h>
#endif

namespace ComputeCardHAL::Audio {

namespace {

#if defined(ESP_PLATFORM)
i2s_chan_handle_t txChannel = nullptr;
bool txChannelEnabled = false;
#endif
uint8_t volume = 100;
OutputMode outputMode = OutputMode::Speaker;
bool codecProbeAttempted = false;
bool codecAvailable = false;

#if defined(ESP_PLATFORM)
constexpr char kLogTag[] = "ComputeCardAudio";
#endif

#if defined(ESP_PLATFORM)

constexpr i2s_port_t kAudioPort = I2S_NUM_1;
constexpr uint32_t kSampleRate = 44100;
constexpr size_t kDmaFrameCount = 256;
constexpr size_t kDmaDescriptorCount = 8;

esp_err_t EnsureI2sInitialized()
{
    if (txChannel != nullptr)
    {
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

    standardConfig.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;

    status = i2s_channel_init_std_mode(txChannel, &standardConfig);
    if (status != ESP_OK)
    {
        i2s_del_channel(txChannel);
        txChannel = nullptr;
    }

    return status;
}

#endif

bool EnsureCodecReady()
{
#if defined(ESP_PLATFORM)
    if (codecAvailable)
    {
        return true;
    }

    if (codecProbeAttempted)
    {
        return false;
    }

    codecProbeAttempted = true;

    if (ComputeCardHAL::System::GetI2CBus() == nullptr)
    {
        if (!ComputeCardHAL::System::Init())
        {
#if defined(ESP_PLATFORM)
            ESP_LOGE(kLogTag, "Failed to initialize I2C bus for audio codec");
#endif
            return false;
        }
    }

    if (!Codec::ES8156::Init())
    {
    #if defined(ESP_PLATFORM)
        ESP_LOGW(kLogTag, "ES8156 codec unavailable, continuing with I2S only");
#endif
        return false;
    }

    codecAvailable = true;
    return true;
#else
    return true;
#endif
}

bool ApplyCachedOutputMode()
{
    if (!codecAvailable)
    {
        return true;
    }

    return Codec::ES8156::SetEnabled(outputMode == OutputMode::Speaker);
}

bool ApplyCachedVolume()
{
    if (!codecAvailable)
    {
        return true;
    }

    return Codec::ES8156::SetVolume(volume);
}

}  // namespace

bool Init()
{
#if defined(ESP_PLATFORM)
    return EnsureI2sInitialized() == ESP_OK;
#endif

    return true;
}

bool NotifyOutputClockStarted()
{
#if defined(ESP_PLATFORM)
    if (!EnsureCodecReady())
    {
        // The board can still stream I2S even when codec control is unavailable.
        codecAvailable = false;
    }

    if (!ApplyCachedOutputMode())
    {
        codecAvailable = false;
    }

    if (!ApplyCachedVolume())
    {
        codecAvailable = false;
    }
#endif
    return true;
}

bool IsOutputReady()
{
#if defined(ESP_PLATFORM)
    return Init() && txChannel != nullptr;
#else
    return false;
#endif
}

bool EnableOutput()
{
#if defined(ESP_PLATFORM)
    if (!IsOutputReady())
    {
        return false;
    }

    if (txChannelEnabled)
    {
        return true;
    }

    if (i2s_channel_enable(txChannel) != ESP_OK)
    {
        return false;
    }

    txChannelEnabled = true;
    return true;
#else
    return false;
#endif
}

bool WriteOutputFrames(const std::int16_t* samples, std::size_t sampleCount)
{
#if defined(ESP_PLATFORM)
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
#else
    (void)samples;
    (void)sampleCount;
    return false;
#endif
}

bool SetVolume(uint8_t value)
{
    if (value > 100)
    {
        value = 100;
    }

    volume = value;
    return ApplyCachedVolume();
}

uint8_t GetVolume()
{
    return volume;
}

bool SetOutputMode(OutputMode mode)
{
    outputMode = mode;
    return ApplyCachedOutputMode();
}

OutputMode GetOutputMode()
{
    return outputMode;
}

}  // namespace ComputeCardHAL::Audio
