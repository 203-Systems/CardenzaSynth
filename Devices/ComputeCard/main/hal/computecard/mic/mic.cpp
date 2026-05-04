#include "Devices/ComputeCard/main/hal/computecard/mic/mic.h"
#include "Devices/ComputeCard/main/hal/computecard/board/pins.h"

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>) && __has_include(<driver/i2s_pdm.h>)
#include <driver/gpio.h>
#include <driver/i2s_pdm.h>
#include <driver/i2s_std.h>
#endif

namespace ComputeCardHAL::Mic {

namespace {

i2s_chan_handle_t micChannel = nullptr;

#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>) && __has_include(<driver/i2s_pdm.h>)

constexpr i2s_port_t kMicPort = I2S_NUM_0;
constexpr uint32_t kSampleRate = 16000;
constexpr size_t kDmaFrameCount = 64;
constexpr size_t kDmaDescriptorCount = 4;

esp_err_t EnsureInitialized()
{
    if (micChannel != nullptr)
    {
        return ESP_OK;
    }

    i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(kMicPort, I2S_ROLE_MASTER);
    channelConfig.dma_desc_num = kDmaDescriptorCount;
    channelConfig.dma_frame_num = kDmaFrameCount;
    channelConfig.auto_clear = false;

    esp_err_t status = i2s_new_channel(&channelConfig, nullptr, &micChannel);
    if (status != ESP_OK)
    {
        micChannel = nullptr;
        return status;
    }

    i2s_pdm_rx_config_t pdmConfig = {
        .clk_cfg = {
            .sample_rate_hz = kSampleRate,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg = {
            .clk = static_cast<gpio_num_t>(Pins::kMicWs),
            .din = static_cast<gpio_num_t>(Pins::kMicDataIn),
            .invert_flags = {
                .clk_inv = false,
            },
        },
    };

    status = i2s_channel_init_pdm_rx_mode(micChannel, &pdmConfig);
    if (status != ESP_OK)
    {
        i2s_del_channel(micChannel);
        micChannel = nullptr;
    }

    return status;
}

#endif

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>) && __has_include(<driver/i2s_pdm.h>)
    return EnsureInitialized();
#else
    return ESP_OK;
#endif
}

i2s_chan_handle_t GetMicChannel()
{
#if __has_include(<driver/gpio.h>) && __has_include(<driver/i2s_std.h>) && __has_include(<driver/i2s_pdm.h>)
    if (micChannel == nullptr)
    {
        (void)EnsureInitialized();
    }
#endif
    return micChannel;
}

}  // namespace ComputeCardHAL::Mic
