#include "Devices/ComputeCard/main/hal/computecard/storage/storage.h"
#include "Devices/ComputeCard/main/hal/computecard/board/pins.h"

#if __has_include(<driver/sdmmc_host.h>) && __has_include(<driver/sdspi_host.h>) && __has_include(<driver/spi_master.h>) && __has_include(<esp_vfs_fat.h>) && __has_include(<sdmmc_cmd.h>)
#include <driver/sdmmc_host.h>
#include <driver/sdspi_host.h>
#include <driver/spi_master.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#endif

namespace ComputeCardHAL::Storage {

namespace {

bool mounted = false;
uint64_t capacity = 0;
sdmmc_card_t* card = nullptr;

#if __has_include(<driver/sdmmc_host.h>) && __has_include(<driver/sdspi_host.h>) && __has_include(<driver/spi_master.h>) && __has_include(<esp_vfs_fat.h>) && __has_include(<sdmmc_cmd.h>)

constexpr char kMountPoint[] = "/sdcard";

#endif

}  // namespace

esp_err_t Init()
{
#if __has_include(<driver/sdmmc_host.h>) && __has_include(<driver/sdspi_host.h>) && __has_include(<driver/spi_master.h>) && __has_include(<esp_vfs_fat.h>) && __has_include(<sdmmc_cmd.h>)
    if (mounted)
    {
        return ESP_OK;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();

    spi_bus_config_t busConfig = {};
    busConfig.mosi_io_num = Pins::kSdSpiMosi;
    busConfig.miso_io_num = Pins::kSdSpiMiso;
    busConfig.sclk_io_num = Pins::kSdSpiSclk;
    busConfig.quadwp_io_num = -1;
    busConfig.quadhd_io_num = -1;
    busConfig.max_transfer_sz = 4000;
    busConfig.flags = SPICOMMON_BUSFLAG_SCLK | SPICOMMON_BUSFLAG_MOSI | SPICOMMON_BUSFLAG_MISO;

    esp_err_t status = spi_bus_initialize(static_cast<spi_host_device_t>(host.slot), &busConfig, SDSPI_DEFAULT_DMA);
    if (status != ESP_OK)
    {
        return status;
    }

    sdspi_device_config_t slotConfig = SDSPI_DEVICE_CONFIG_DEFAULT();
    slotConfig.gpio_cs = static_cast<gpio_num_t>(Pins::kSdSpiCs);
    slotConfig.host_id = static_cast<spi_host_device_t>(host.slot);

    esp_vfs_fat_sdmmc_mount_config_t mountConfig = {};
    mountConfig.format_if_mount_failed = false;
    mountConfig.max_files = 5;
    mountConfig.allocation_unit_size = 16 * 1024;

    status = esp_vfs_fat_sdspi_mount(kMountPoint, &host, &slotConfig, &mountConfig, &card);
    if (status != ESP_OK)
    {
        card = nullptr;
        (void)spi_bus_free(static_cast<spi_host_device_t>(host.slot));
        return status;
    }

    mounted = true;
    capacity = static_cast<uint64_t>(card->csd.capacity) * card->csd.sector_size;
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

bool IsMounted()
{
    return mounted;
}

uint64_t GetCapacity()
{
    return capacity;
}

sdmmc_card_t* GetCard()
{
    return card;
}

}  // namespace ComputeCardHAL::Storage
