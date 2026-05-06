#include "es8156.h"

#include "../../system/system_internal.h"

#if defined(ESP_PLATFORM)
#include <esp_log.h>
#endif

#if defined(ESP_PLATFORM)
#include <driver/i2c_master.h>
#endif

namespace ComputeCardHAL::Audio::Codec::ES8156 {

namespace {

bool g_initialized = false;
bool g_enabled = false;
uint8_t g_volume = 100;

#if defined(ESP_PLATFORM)

struct RegisterValue {
    uint8_t reg;
    uint8_t value;
};

constexpr uint16_t kCodecAddress = 0x08;
constexpr uint16_t kI2cScanStartAddress = 0x03;
constexpr uint16_t kI2cScanEndAddress = 0x77;
constexpr uint32_t kI2cSpeedHz = 100000;
constexpr int kI2cTimeoutMs = 250;
constexpr int kI2cProbeTimeoutMs = 100;
constexpr int kI2cScanTimeoutMs = 20;
constexpr uint8_t kModeConfigUseSclkSoftwareSlave = 0x84;
// ESP-IDF standard mode is fixed to 2 slots. With 16-bit samples that yields BCK = 32 * Fs,
// so the ES8156 must follow the vendor Ratio=128 path when it derives its clock from SCLK.
constexpr uint8_t kClockControlRatio128 = 0x61;
constexpr uint8_t kMasterLrckDividerHigh = 0x00;
constexpr uint8_t kMasterLrckDividerLow = 0x80;
constexpr uint8_t kMasterSclkDivider = 0x04;
constexpr uint8_t kMuteControlNormal = 0x00;
constexpr uint8_t kInterfaceConfigI2s16Bit = 0x30;
constexpr uint8_t kLineOutAnalogSystem2 = 0x02;
constexpr uint8_t kLineOutAnalogSystem3 = 0x40;
constexpr uint8_t kVolumeRegister = 0x14;
constexpr uint8_t kUnityGainRegister = 0xBF;
constexpr uint8_t kChipIdRegisterMsb = 0xFD;
constexpr uint8_t kChipIdRegisterMid = 0xFE;
constexpr uint8_t kChipIdRegisterLsb = 0xFF;
constexpr uint8_t kStateConfirmRegister = 0x0C;
constexpr uint8_t kExpectedChipIdMsb = 0x81;
constexpr uint8_t kExpectedChipIdMid = 0x55;
constexpr uint8_t kExpectedStateConfirm = 0x60;

constexpr RegisterValue kInitSequence[] = {
    {0x02, kModeConfigUseSclkSoftwareSlave},
    {0x13, kMuteControlNormal},
    {0x20, 0x2A},
    {0x21, 0x3C},
    {0x22, kLineOutAnalogSystem2},
    {0x24, 0x07},
    {0x23, kLineOutAnalogSystem3},
    {0x0A, 0x01},
    {0x0B, 0x01},
    {0x11, kInterfaceConfigI2s16Bit},
    {0x14, kUnityGainRegister},
    {0x01, kClockControlRatio128},
    {0x09, 0x00},
    {0x03, kMasterLrckDividerHigh},
    {0x04, kMasterLrckDividerLow},
    {0x05, kMasterSclkDivider},
    {0x0D, 0x14},
    {0x18, 0x00},
    {0x08, 0x3F},
    {0x00, 0x02},
    {0x00, 0x03},
    {0x25, 0x20},
};

constexpr RegisterValue kStandbySequence[] = {
    {0x14, 0x00},
    {0x19, 0x02},
    {0x25, 0xA1},
    {0x18, 0x01},
    {0x09, 0x02},
    {0x09, 0x01},
    {0x08, 0x00},
};

i2c_master_dev_handle_t g_codecHandle = nullptr;

#if defined(ESP_PLATFORM)
constexpr char kLogTag[] = "ES8156";
constexpr char kI2cMasterLogTag[] = "i2c.master";
#endif

#ifdef ESP_ERR_INVALID_STATE
constexpr esp_err_t kInvalidState = ESP_ERR_INVALID_STATE;
#else
constexpr esp_err_t kInvalidState = ESP_FAIL;
#endif

esp_err_t EnsureDeviceHandle();
esp_err_t ReadRegister(uint8_t reg, uint8_t* value);

void LogIdentityRegisters()
{
#if defined(ESP_PLATFORM)
    uint8_t chipIdMsb = 0;
    uint8_t chipIdMid = 0;
    uint8_t chipIdLsb = 0;
    uint8_t stateConfirm = 0;

    const esp_err_t chipIdStatus = ReadRegister(kChipIdRegisterMsb, &chipIdMsb);
    const esp_err_t chipIdMidStatus = ReadRegister(kChipIdRegisterMid, &chipIdMid);
    const esp_err_t chipIdLsbStatus = ReadRegister(kChipIdRegisterLsb, &chipIdLsb);
    const esp_err_t stateStatus = ReadRegister(kStateConfirmRegister, &stateConfirm);
    if (chipIdStatus != ESP_OK || chipIdMidStatus != ESP_OK || chipIdLsbStatus != ESP_OK || stateStatus != ESP_OK)
    {
        ESP_LOGW(kLogTag, "Failed to read ES8156 identity/state registers after probe");
        return;
    }

    ESP_LOGI(kLogTag, "ES8156 ID/state: 0x%02X 0x%02X 0x%02X, state 0x%02X",
             chipIdMsb,
             chipIdMid,
             chipIdLsb,
             stateConfirm);

    if (chipIdMsb != kExpectedChipIdMsb || chipIdMid != kExpectedChipIdMid)
    {
        ESP_LOGW(kLogTag, "ES8156 ID prefix mismatch, expected 0x%02X 0x%02X xx from the datasheet",
                 kExpectedChipIdMsb,
                 kExpectedChipIdMid);
    }

    if (stateConfirm != kExpectedStateConfirm)
    {
        ESP_LOGW(kLogTag, "ES8156 state register is 0x%02X, datasheet normal-state value is 0x%02X",
                 stateConfirm,
                 kExpectedStateConfirm);
    }
#endif
}

void LogProbeResults()
{
#if defined(ESP_PLATFORM)
    static bool probeLogged = false;
    if (probeLogged)
    {
        return;
    }
    probeLogged = true;

    i2c_master_bus_handle_t bus = ComputeCardHAL::System::GetI2CBus();
    if (bus == nullptr)
    {
        ESP_LOGW(kLogTag, "I2C probe skipped because the external bus is unavailable");
        return;
    }

    const esp_err_t codecProbeStatus = i2c_master_probe(bus, kCodecAddress, kI2cProbeTimeoutMs);
    if (codecProbeStatus == ESP_OK)
    {
        ESP_LOGI(kLogTag, "I2C probe: ES8156 responded at 0x%02X", kCodecAddress);
        LogIdentityRegisters();
    }
    else
    {
        ESP_LOGW(kLogTag, "I2C probe: ES8156 not found at 0x%02X: %s", kCodecAddress, esp_err_to_name(codecProbeStatus));
    }

    const esp_log_level_t previousI2cMasterLogLevel = esp_log_level_get(kI2cMasterLogTag);
    esp_log_level_set(kI2cMasterLogTag, ESP_LOG_NONE);

    int foundCount = 0;
    int timeoutCount = 0;
    int unexpectedCount = 0;
    uint16_t firstTimeoutAddress = 0;
    uint16_t lastTimeoutAddress = 0;
    uint16_t firstUnexpectedAddress = 0;
    esp_err_t firstUnexpectedStatus = ESP_OK;

    for (uint16_t address = kI2cScanStartAddress; address <= kI2cScanEndAddress; ++address)
    {
        const esp_err_t scanStatus = i2c_master_probe(bus, address, kI2cScanTimeoutMs);
        if (scanStatus == ESP_OK)
        {
            ++foundCount;
            ESP_LOGI(kLogTag, "I2C scan: found device at 0x%02X", address);
        }
        else if (scanStatus == ESP_ERR_TIMEOUT)
        {
            ++timeoutCount;
            if (timeoutCount == 1)
            {
                firstTimeoutAddress = address;
            }
            lastTimeoutAddress = address;
        }
        else if (scanStatus != ESP_ERR_NOT_FOUND)
        {
            ++unexpectedCount;
            if (unexpectedCount == 1)
            {
                firstUnexpectedAddress = address;
                firstUnexpectedStatus = scanStatus;
            }
        }
    }

    esp_log_level_set(kI2cMasterLogTag, previousI2cMasterLogLevel);

    if (foundCount == 0)
    {
        ESP_LOGW(kLogTag, "I2C scan: no responding devices found on the shared external bus (0x%02X-0x%02X)", kI2cScanStartAddress, kI2cScanEndAddress);
    }
    else
    {
        ESP_LOGI(kLogTag, "I2C scan: found %d responding device(s) on the shared external bus", foundCount);
    }

    if (timeoutCount > 0)
    {
        const int totalScannedAddresses = kI2cScanEndAddress - kI2cScanStartAddress + 1;
        if (timeoutCount == totalScannedAddresses)
        {
            ESP_LOGW(kLogTag, "I2C scan: every probed address timed out across 0x%02X-0x%02X", kI2cScanStartAddress, kI2cScanEndAddress);
        }
        else
        {
            ESP_LOGW(kLogTag, "I2C scan: timeout on %d address(es), first 0x%02X last 0x%02X", timeoutCount, firstTimeoutAddress, lastTimeoutAddress);
        }
    }

    if (unexpectedCount > 0)
    {
        ESP_LOGW(kLogTag, "I2C scan: unexpected status on %d address(es), first 0x%02X -> %s", unexpectedCount, firstUnexpectedAddress, esp_err_to_name(firstUnexpectedStatus));
    }
#endif
}

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
#if defined(ESP_PLATFORM)
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
#if defined(ESP_PLATFORM)
        ESP_LOGE(kLogTag, "Failed to write ES8156 reg 0x%02X: %s", reg, esp_err_to_name(status));
#endif
    }
    return status;
}

esp_err_t ReadRegister(uint8_t reg, uint8_t* value)
{
    if (value == nullptr)
    {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_err_t handleStatus = EnsureDeviceHandle();
    if (handleStatus != ESP_OK)
    {
        return handleStatus;
    }

    return i2c_master_transmit_receive(g_codecHandle, &reg, sizeof(reg), value, 1, kI2cTimeoutMs);
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

esp_err_t WriteStartupSequence()
{
    return WriteSequence(kInitSequence, sizeof(kInitSequence) / sizeof(kInitSequence[0]));
}

uint8_t MapVolumeRegister(uint8_t volume)
{
    return static_cast<uint8_t>((static_cast<unsigned>(volume) * kUnityGainRegister + 50U) / 100U);
}

#endif

}  // namespace

bool Init()
{
#if defined(ESP_PLATFORM)
    if (g_initialized)
    {
        return SetEnabled(true);
    }

    LogProbeResults();

    const esp_err_t status = WriteStartupSequence();
    if (status != ESP_OK)
    {
        return false;
    }

    g_initialized = true;
    g_enabled = true;
    return SetVolume(g_volume);
#else
    return true;
#endif
}

bool SetEnabled(bool enabled)
{
#if defined(ESP_PLATFORM)
    if (enabled && !g_initialized)
    {
        return Init();
    }

    if (!g_initialized)
    {
        g_enabled = false;
        return true;
    }

    if (g_enabled == enabled)
    {
        return true;
    }

    const esp_err_t status = enabled
        ? WriteStartupSequence()
        : WriteSequence(kStandbySequence, sizeof(kStandbySequence) / sizeof(kStandbySequence[0]));
    if (status != ESP_OK)
    {
        return false;
    }

    g_enabled = enabled;
    if (g_enabled)
    {
        return SetVolume(g_volume);
    }
    return true;
#else
    (void)enabled;
    return true;
#endif
}

bool IsEnabled()
{
    return g_enabled;
}

bool SetVolume(uint8_t volume)
{
    if (volume > 100)
    {
        volume = 100;
    }

    g_volume = volume;

#if defined(ESP_PLATFORM)
    if (!g_initialized)
    {
        return true;
    }

    return WriteRegister(kVolumeRegister, MapVolumeRegister(volume)) == ESP_OK;
#else
    return true;
#endif
}

uint8_t GetVolume()
{
    return g_volume;
}

}  // namespace ComputeCardHAL::Audio::Codec::ES8156