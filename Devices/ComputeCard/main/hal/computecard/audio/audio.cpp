#include "Devices/ComputeCard/main/hal/computecard/audio/audio.h"
#include "Devices/ComputeCard/main/hal/computecard/audio/codec/es8156.h"
#include "Devices/ComputeCard/main/hal/computecard/board/pins.h"
#include "Devices/ComputeCard/main/hal/computecard/system/system.h"

#if __has_include(<esp_log.h>)
#include <esp_log.h>
#endif

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#endif

namespace ComputeCardHAL::Audio {

namespace {

i2s_chan_handle_t txChannel = nullptr;
uint8_t volume = 100;
OutputMode outputMode = OutputMode::Speaker;

#if __has_include(<esp_log.h>)
constexpr char kLogTag[] = "ComputeCardAudio";
#endif

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)

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

esp_err_t EnsureCodecReady()
{
#if __has_include(<driver/i2c_master.h>)
    if (ComputeCardHAL::System::GetI2CBus() == nullptr)
    {
        const esp_err_t systemStatus = ComputeCardHAL::System::Init();
        if (systemStatus != ESP_OK)
        {
#if __has_include(<esp_log.h>)
            ESP_LOGE(kLogTag, "Failed to initialize I2C bus for audio codec: %s", esp_err_to_name(systemStatus));
#endif
            return systemStatus;
        }
    }

    const esp_err_t codecStatus = Codec::ES8156::Init();
    if (codecStatus != ESP_OK)
    {
#if __has_include(<esp_log.h>)
        ESP_LOGE(kLogTag, "Failed to initialize ES8156 codec: %s", esp_err_to_name(codecStatus));
#endif
    }
    return codecStatus;
#else
    return ESP_OK;
#endif
}

esp_err_t ApplyCachedOutputMode()
{
    return Codec::ES8156::SetEnabled(outputMode == OutputMode::Speaker);
}

esp_err_t ApplyCachedVolume()
{
    return Codec::ES8156::SetVolume(volume);
}

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
    const esp_err_t i2sStatus = EnsureI2sInitialized();
    if (i2sStatus != ESP_OK)
    {
        return i2sStatus;
    }
#endif

#if __has_include(<driver/i2c_master.h>)
    const esp_err_t codecStatus = EnsureCodecReady();
    if (codecStatus != ESP_OK)
    {
        return codecStatus;
    }

    const esp_err_t outputStatus = ApplyCachedOutputMode();
    if (outputStatus != ESP_OK)
    {
        return outputStatus;
    }

    const esp_err_t volumeStatus = ApplyCachedVolume();
    if (volumeStatus != ESP_OK)
    {
        return volumeStatus;
    }

    return ESP_OK;
#else
    return ESP_OK;
#endif
}

i2s_chan_handle_t GetTxChannel()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
    if (txChannel == nullptr)
    {
        (void)Init();
    }
#endif
    return txChannel;
}

i2s_chan_handle_t GetAudioChannel()
{
    return GetTxChannel();
}

esp_err_t SetVolume(uint8_t value)
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

esp_err_t SetOutputMode(OutputMode mode)
{
    outputMode = mode;
    return ApplyCachedOutputMode();
}

OutputMode GetOutputMode()
{
    return outputMode;
}

}  // namespace ComputeCardHAL::Audio
