#include "Devices/ComputeCard/main/hal/computecard/audio/audio.h"
#include "Devices/ComputeCard/main/hal/computecard/board/pins.h"

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
#include <driver/gpio.h>
#include <driver/i2s_std.h>
#endif

namespace ComputeCardHAL::Audio {

namespace {

i2s_chan_handle_t audioChannel = nullptr;
uint8_t volume = 100;

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)

constexpr i2s_port_t kAudioPort = I2S_NUM_1;
constexpr uint32_t kSampleRate = 44100;
constexpr size_t kDmaFrameCount = 256;
constexpr size_t kDmaDescriptorCount = 8;

esp_err_t EnsureInitialized()
{
    if (audioChannel != nullptr)
    {
        return ESP_OK;
    }

    i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(kAudioPort, I2S_ROLE_MASTER);
    channelConfig.dma_desc_num = kDmaDescriptorCount;
    channelConfig.dma_frame_num = kDmaFrameCount;
    channelConfig.auto_clear = true;

    esp_err_t status = i2s_new_channel(&channelConfig, &audioChannel, nullptr);
    if (status != ESP_OK)
    {
        audioChannel = nullptr;
        return status;
    }

    i2s_std_config_t standardConfig = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate),
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

    status = i2s_channel_init_std_mode(audioChannel, &standardConfig);
    if (status != ESP_OK)
    {
        i2s_del_channel(audioChannel);
        audioChannel = nullptr;
    }

    return status;
}

#endif

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
    return EnsureInitialized();
#else
    return ESP_OK;
#endif
}

i2s_chan_handle_t GetAudioChannel()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>)
    if (audioChannel == nullptr)
    {
        (void)EnsureInitialized();
    }
#endif
    return audioChannel;
}

esp_err_t SetVolume(uint8_t value)
{
    volume = value;
    return ESP_OK;
}

uint8_t GetVolume()
{
    return volume;
}

}  // namespace ComputeCardHAL::Audio
