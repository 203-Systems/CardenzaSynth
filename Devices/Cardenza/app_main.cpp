#include <esp_heap_caps.h>
#include <esp_heap_caps_init.h>
#include <esp_log.h>
#include <esp_mmu_map.h>
#include <sdkconfig.h>
#if CONFIG_SPIRAM
#include <esp_psram.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "Devices/Cardenza/DeviceLog.h"
#include "Devices/Device.h"
#include "OS/System/System.h"

namespace {

constexpr char kLogTag[] = "Cardenza";
// A frame every 10 ms (keys, LEDs, the app's tick); the screen is drawn every other one, 50 fps.
// pdMS_TO_TICKS rounds down: 16 ms was one 10 ms tick at 100 Hz.
constexpr TickType_t kFrameTick = pdMS_TO_TICKS(10);
static_assert(kFrameTick >= 1, "the frame tick needs a tick rate of 100 Hz or more");
// ESP-IDF's task creation APIs take bytes, unlike upstream FreeRTOS.
constexpr uint32_t kRuntimeTaskStackBytes = 16384;
constexpr UBaseType_t kRuntimeTaskPriority = 5;
constexpr BaseType_t kRuntimeTaskPinnedCore = 0;

// Internal RAM is the scarce resource, so log headroom at boot; PSRAM beside it.
void LogInternalHeap(const char* stage)
{
    constexpr uint32_t kCaps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    ESP_LOGI(kLogTag,
             "heap %s: free=%u largest=%u min=%u psram=%u of %u",
             stage,
             static_cast<unsigned>(heap_caps_get_free_size(kCaps)),
             static_cast<unsigned>(heap_caps_get_largest_free_block(kCaps)),
             static_cast<unsigned>(heap_caps_get_minimum_free_size(kCaps)),
             static_cast<unsigned>(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)),
             static_cast<unsigned>(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));
}

// The chip's PSRAM is mapped (CONFIG_SPIRAM_USE_MEMMAP) but not in the heap: added here with
// MALLOC_CAP_SPIRAM alone, so only what asks for it lands there (Library/Memory/Memory.h). With
// ESP-IDF's own integration it would answer MALLOC_CAP_DEFAULT before internal RAM, and the drivers
// whose interrupts are not IRAM-safe (the key and LED scan timer, I2S) would keep their state in it.
void AddPsramToHeap()
{
#if CONFIG_SPIRAM
    if (!esp_psram_is_initialized())
    {
        ESP_LOGW(kLogTag, "no PSRAM: everything stays in internal RAM");
        return;
    }
    const size_t size = esp_psram_get_size();
    void* first = nullptr;
    void* last = nullptr;
    if (size == 0 || esp_mmu_paddr_to_vaddr(0, MMU_TARGET_PSRAM0, MMU_VADDR_DATA, &first) != ESP_OK ||
        esp_mmu_paddr_to_vaddr(size - 1, MMU_TARGET_PSRAM0, MMU_VADDR_DATA, &last) != ESP_OK ||
        static_cast<char*>(last) != static_cast<char*>(first) + size - 1)
    {
        ESP_LOGE(kLogTag, "PSRAM of %u bytes is not mapped in one piece: not used", static_cast<unsigned>(size));
        return;
    }
    const uint32_t caps[] = {MALLOC_CAP_SPIRAM, 0, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT};
    const intptr_t start = reinterpret_cast<intptr_t>(first);
    if (heap_caps_add_region_with_caps(caps, start, start + static_cast<intptr_t>(size)) != ESP_OK)
    {
        ESP_LOGE(kLogTag, "PSRAM could not be added to the heap");
        return;
    }
    ESP_LOGI(kLogTag, "PSRAM: %u bytes at %p for slow data", static_cast<unsigned>(size), first);
#endif
}

void LogFailedAllocation(size_t size, uint32_t caps, const char* functionName)
{
    ESP_EARLY_LOGE(kLogTag, "alloc failed: %u bytes caps=0x%x in %s", static_cast<unsigned>(size),
                   static_cast<unsigned>(caps), functionName != nullptr ? functionName : "?");
    Device::Log::NoteFailedAllocation(size, caps, functionName);
}

void CardenzaRuntimeTask(void*)
{
    LogInternalHeap("before boot");
    if (!os::system::Begin())
    {
        ESP_LOGE(kLogTag, "Failed to boot Cardenza runtime");
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI(kLogTag,
             "%s [%s] %dx%d",
             Device::Name().c_str(),
             Device::BackendName().c_str(),
             Device::Display::kWidth,
             Device::Display::kHeight);
    LogInternalHeap("after boot");

    TickType_t lastWakeTime = xTaskGetTickCount();
    for (std::uint32_t frame = 0;; ++frame)
    {
        os::system::RunFrame(frame % 2 == 0);
        vTaskDelayUntil(&lastWakeTime, kFrameTick);
    }
}

}  // namespace

extern "C" void app_main(void)
{
    // Before any task exists: what the previous run left in the log ring, and the console through it.
    Device::Log::Install();
    AddPsramToHeap();
    heap_caps_register_failed_alloc_callback(LogFailedAllocation);
    BaseType_t created = pdFAIL;
#if portNUM_PROCESSORS > 1
    created = xTaskCreatePinnedToCore(
        CardenzaRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackBytes,
        nullptr,
        kRuntimeTaskPriority,
        nullptr,
        kRuntimeTaskPinnedCore);
#else
    created = xTaskCreate(
        CardenzaRuntimeTask,
        "cc_runtime",
        kRuntimeTaskStackBytes,
        nullptr,
        kRuntimeTaskPriority,
        nullptr);
#endif

    if (created != pdPASS)
    {
        ESP_LOGE(kLogTag, "Failed to create Cardenza runtime task");
        return;
    }
}
