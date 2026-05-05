#include "Devices/ComputeCard/main/hal/computecard/audio/codec/es8156.h"

#include "Devices/ComputeCard/main/hal/computecard/system/system.h"

#if __has_include(<esp_log.h>)
#include <esp_log.h>
#endif

#if __has_include(<driver/i2c_master.h>)
#include <driver/i2c_master.h>
#endif

namespace ComputeCardHAL::Audio::Codec::ES8156 {

namespace {

#if __has_include(<driver/i2c_master.h>)

struct RegisterValue {
    uint8_t reg;
    uint8_t value;
};

constexpr uint16_t kCodecAddress = 0x10;
constexpr uint32_t kI2cSpeedHz = 100000;
constexpr int kI2cTimeoutMs = 100;
constexpr uint8_t kVolumeRegister = 0x14;
constexpr uint8_t kUnityGainRegister = 0xBF;

constexpr RegisterValue kInitSequence[] = {
    {0x02, 0x04},
    {0x20, 0x2A},
    {0x21, 0x3C},
    {0x22, 0x00},
    {0x24, 0x07},
    {0x23, 0x00},
    {0x0A, 0x01},
    {0x0B, 0x01},
    {0x11, 0x00},
    {0x0D, 0x14},
    {0x18, 0x00},
    {0x08, 0x3F},
    {0x00, 0x02},
    {0x00, 0x03},
    {0x25, 0x20},
};

constexpr RegisterValue kResumeSequence[] = {
    {0x08, 0x3F},
    {0x09, 0x00},
    {0x18, 0x00},
    {0x25, 0x20},
    {0x22, 0x00},
    {0x21, 0x3C},
    {0x19, 0x20},
};

constexpr RegisterValue kStandbySequence[] = {
    {0x14, 0x00},
    {0x19, 0x02},
    {0x21, 0x1F},
    {0x22, 0x02},
    {0x25, 0x21},
    {0x25, 0xA1},
    {0x18, 0x01},
    {0x09, 0x02},
    {0x09, 0x01},
    {0x08, 0x00},
};

i2c_master_dev_handle_t g_codecHandle = nullptr;
bool g_initialized = false;
bool g_enabled = false;
uint8_t g_volume = 100;

#if __has_include(<esp_log.h>)
constexpr char kLogTag[] = "ES8156";
#endif

#ifdef ESP_ERR_INVALID_STATE
constexpr esp_err_t kInvalidState = ESP_ERR_INVALID_STATE;
#else
constexpr esp_err_t kInvalidState = ESP_FAIL;
#endif

esp_err_t EnsureDeviceHandle()
{
    if (g_codecHandle != nullptr)
    {
        return ESP_OK;
    }

    i2c_master_bus_handle_t bus = ComputeCardHAL::System::GetI2CBus();
    if (bus == nullptr)
    {
        return kInvalidState;
    }

    i2c_device_config_t deviceConfig = {};
    deviceConfig.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    deviceConfig.device_address = kCodecAddress;
    deviceConfig.scl_speed_hz = kI2cSpeedHz;
    deviceConfig.scl_wait_us = 0;

    const esp_err_t status = i2c_master_bus_add_device(bus, &deviceConfig, &g_codecHandle);
    if (status != ESP_OK)
    {
#if __has_include(<esp_log.h>)
        ESP_LOGE(kLogTag, "Failed to bind ES8156 on I2C bus: %s", esp_err_to_name(status));
#endif
        g_codecHandle = nullptr;
    }
    return status;
}

esp_err_t WriteRegister(uint8_t reg, uint8_t value)
{
    const esp_err_t handleStatus = EnsureDeviceHandle();
    if (handleStatus != ESP_OK)
    {
        return handleStatus;
    }

    const uint8_t payload[] = {reg, value};
    const esp_err_t status = i2c_master_transmit(g_codecHandle, payload, sizeof(payload), kI2cTimeoutMs);
    if (status != ESP_OK)
    {
#if __has_include(<esp_log.h>)
        ESP_LOGE(kLogTag, "Failed to write ES8156 reg 0x%02X: %s", reg, esp_err_to_name(status));
#endif
    }
    return status;
}

esp_err_t WriteSequence(const RegisterValue* sequence, size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        const esp_err_t status = WriteRegister(sequence[index].reg, sequence[index].value);
        if (status != ESP_OK)
        {
            return status;
        }
    }
    return ESP_OK;
}

uint8_t MapVolumeRegister(uint8_t volume)
{
    return static_cast<uint8_t>((static_cast<unsigned>(volume) * kUnityGainRegister + 50U) / 100U);
}

#endif

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/i2c_master.h>)
    if (g_initialized)
    {
        return SetEnabled(true);
    }

    const esp_err_t status = WriteSequence(kInitSequence, sizeof(kInitSequence) / sizeof(kInitSequence[0]));
    if (status != ESP_OK)
    {
        return status;
    }

    g_initialized = true;
    g_enabled = true;
    return SetVolume(g_volume);
#else
    return ESP_OK;
#endif
}

esp_err_t SetEnabled(bool enabled)
{
#if __has_include(<driver/i2c_master.h>)
    if (enabled && !g_initialized)
    {
        return Init();
    }

    if (!g_initialized)
    {
        g_enabled = false;
        return ESP_OK;
    }

    if (g_enabled == enabled)
    {
        return ESP_OK;
    }

    const RegisterValue* sequence = enabled ? kResumeSequence : kStandbySequence;
    const size_t count = enabled
        ? sizeof(kResumeSequence) / sizeof(kResumeSequence[0])
        : sizeof(kStandbySequence) / sizeof(kStandbySequence[0]);

    const esp_err_t status = WriteSequence(sequence, count);
    if (status != ESP_OK)
    {
        return status;
    }

    g_enabled = enabled;
    if (g_enabled)
    {
        return SetVolume(g_volume);
    }
    return ESP_OK;
#else
    (void)enabled;
    return ESP_OK;
#endif
}

bool IsEnabled()
{
    return g_enabled;
}

esp_err_t SetVolume(uint8_t volume)
{
    if (volume > 100)
    {
        volume = 100;
    }

    g_volume = volume;

#if __has_include(<driver/i2c_master.h>)
    if (!g_initialized)
    {
        return ESP_OK;
    }

    return WriteRegister(kVolumeRegister, MapVolumeRegister(volume));
#else
    return ESP_OK;
#endif
}

uint8_t GetVolume()
{
    return g_volume;
}

}  // namespace ComputeCardHAL::Audio::Codec::ES8156